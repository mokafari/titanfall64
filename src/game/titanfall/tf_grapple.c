/*
 * tf_grapple.c -- Titanfall64 grapple/hook system
 *
 * Fires a ray from Mario's head in camera direction. If geometry is hit
 * within TF_GRAPPLE_MAX_RANGE, Mario is pulled toward that point with
 * swing physics: lateral momentum is preserved, gravity is reduced to 30%,
 * and air strafing works at 30% strength.
 *
 * Raycast is done by stepping along the ray and checking floor/ceil/wall
 * collisions at each step (no find_surface_on_ray in this SM64 port).
 *
 * R button (R_TRIG on controller) fires grapple.
 * Release R to detach early; auto-detach within 80 units or after 90 frames.
 * Speed boost (1.1x) on detach. 30-frame cooldown between grapples.
 */

#include <math.h>
#include "sm64.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "game/mario.h"
#include "game/titanfall/tf_movement.h"
#include "game/titanfall/tf_math.h"
#include "game/titanfall/tf_grapple.h"
#include "sounds.h"
#include "audio/external.h"

/* ── Step-based raycast ────────────────────────────────────────────── */

/*
 * tf_raycast_step -- Walk along a ray checking for geometry.
 *
 * At each step we check:
 *   1. Floor height at (x, z) -- if ray goes below floor, it hit the floor.
 *   2. Ceiling height at (x, z) -- if ray goes above ceiling, it hit ceiling.
 *   3. Wall collision -- if walls are found within a small radius.
 *
 * Returns 1 if a hit was found, with hitPos filled in.
 * Returns 0 if the ray exhausted its range with no hit.
 */
static s32 tf_raycast_step(Vec3f origin, Vec3f dir, f32 maxDist, Vec3f hitPos) {
    f32 step = TF_GRAPPLE_RAY_STEP;
    s32 numSteps = (s32)(maxDist / step) + 1;
    Vec3f pos;

    vec3f_copy(pos, origin);

    for (s32 i = 1; i <= numSteps; i++) {
        f32 t = step * (f32)i;
        if (t > maxDist) t = maxDist;

        pos[0] = origin[0] + dir[0] * t;
        pos[1] = origin[1] + dir[1] * t;
        pos[2] = origin[2] + dir[2] * t;

        /* Floor check */
        struct Surface *floor = NULL;
        f32 floorY = find_floor(pos[0], pos[1] + 80.0f, pos[2], &floor);
        if (floor != NULL && pos[1] < floorY + 10.0f) {
            hitPos[0] = pos[0];
            hitPos[1] = floorY;
            hitPos[2] = pos[2];
            return 1;
        }

        /* Ceiling check */
        struct Surface *ceil = NULL;
        f32 ceilY = find_ceil(pos[0], pos[1] - 10.0f, pos[2], &ceil);
        if (ceil != NULL && pos[1] > ceilY - 10.0f) {
            hitPos[0] = pos[0];
            hitPos[1] = ceilY;
            hitPos[2] = pos[2];
            return 1;
        }

        /* Wall check */
        struct WallCollisionData wallData;
        wallData.x = pos[0];
        wallData.y = pos[1];
        wallData.z = pos[2];
        wallData.offsetY = 30.0f;
        wallData.radius = 50.0f;
        wallData.numWalls = 0;
        if (find_wall_collisions(&wallData) != 0 && wallData.numWalls > 0) {
            hitPos[0] = wallData.x;
            hitPos[1] = pos[1];
            hitPos[2] = wallData.z;
            return 1;
        }
    }

    return 0;
}

/* ── Fire grapple ──────────────────────────────────────────────────── */

s32 tf_try_grapple(struct MarioState *m, struct GrappleState *gr) {
    /* Already active or on cooldown */
    if (gr->active) return 0;
    if (gr->cooldown > 0) return 0;

    /* R trigger must be pressed (just went down this frame) */
    if (!(m->controller->buttonPressed & R_TRIG)) return 0;

    /* Build camera-forward ray direction */
    s16 yawS16   = (s16)(gTFState.camera.yaw   / 360.0f * 65536.0f);
    s16 pitchS16 = (s16)(gTFState.camera.pitch / 360.0f * 65536.0f);

    Vec3f dir;
    dir[0] =  coss(pitchS16) * sins(yawS16);
    dir[1] = -sins(pitchS16);
    dir[2] =  coss(pitchS16) * coss(yawS16);

    /* Ray origin: Mario's head position */
    Vec3f origin;
    origin[0] = m->pos[0];
    origin[1] = m->pos[1] + 120.0f;  /* roughly head height */
    origin[2] = m->pos[2];

    /* Cast ray */
    Vec3f hitPos;
    if (!tf_raycast_step(origin, dir, TF_GRAPPLE_MAX_RANGE, hitPos)) {
        return 0;  /* nothing in range */
    }

    /* Activate grapple */
    gr->active = 1;
    gr->timer = 0;
    vec3f_copy(gr->targetPos, hitPos);

    /* Compute initial rope length */
    f32 dx = hitPos[0] - m->pos[0];
    f32 dy = hitPos[1] - m->pos[1];
    f32 dz = hitPos[2] - m->pos[2];
    gr->ropeLength = sqrtf(dx * dx + dy * dy + dz * dz);

    play_sound(SOUND_ACTION_TERRAIN_JUMP, m->marioObj->header.gfx.cameraToObject);
    m->particleFlags |= PARTICLE_HORIZONTAL_STAR;

    return 1;
}

/* ── Per-frame grapple update ──────────────────────────────────────── */

void tf_update_grapple(struct MarioState *m, struct GrappleState *gr, f32 dt) {
    if (!gr->active) return;

    gr->timer++;

    /* ── Auto-detach: max duration ──────────────────────── */
    if (gr->timer > TF_GRAPPLE_MAX_DURATION) {
        tf_grapple_detach(m, gr);
        return;
    }

    /* ── Release R to detach ────────────────────────────── */
    if (!(m->controller->buttonDown & R_TRIG)) {
        tf_grapple_detach(m, gr);
        return;
    }

    /* ── Direction to target ────────────────────────────── */
    Vec3f toTarget;
    toTarget[0] = gr->targetPos[0] - m->pos[0];
    toTarget[1] = gr->targetPos[1] - m->pos[1];
    toTarget[2] = gr->targetPos[2] - m->pos[2];

    f32 dist = sqrtf(toTarget[0] * toTarget[0]
                   + toTarget[1] * toTarget[1]
                   + toTarget[2] * toTarget[2]);

    /* ── Auto-detach: close enough ──────────────────────── */
    if (dist < TF_GRAPPLE_MIN_DIST) {
        tf_grapple_detach(m, gr);
        return;
    }

    /* Normalize direction to target */
    f32 invDist = 1.0f / dist;
    toTarget[0] *= invDist;
    toTarget[1] *= invDist;
    toTarget[2] *= invDist;

    /* ── Pull acceleration toward target ────────────────── */
    /*
     * Apply pull force toward the grapple point.
     * Vertical bias: add extra upward component so the grapple
     * arcs upward rather than pulling in a flat line.
     */
    f32 pullForce = TF_GRAPPLE_PULL_FORCE;

    m->vel[0] += toTarget[0] * pullForce;
    m->vel[1] += (toTarget[1] + TF_GRAPPLE_VERTICAL_BIAS) * pullForce;
    m->vel[2] += toTarget[2] * pullForce;

    /* ── Reduced gravity (30% of normal) ────────────────── */
    /*
     * SM64 apply_gravity does ~4.0 per frame.
     * We counteract most of it: apply only 30% worth of gravity.
     * Net gravity = 4.0 * 0.3 = 1.2 per frame.
     * We add back 4.0 * 0.7 = 2.8 to counteract vanilla gravity
     * that will be applied by perform_air_step.
     */
    m->vel[1] += 4.0f * (1.0f - TF_GRAPPLE_GRAVITY_SCALE);

    /* ── Air strafing at 30% strength ───────────────────── */
    {
        Vec3f wishdir;
        f32 wishspeed;
        get_wishdir(m, wishdir, &wishspeed);

        if (wishspeed > 0.01f) {
            /* Project wish onto plane perpendicular to pull direction
             * to get lateral strafe component only */
            f32 dot = wishdir[0] * toTarget[0] + wishdir[2] * toTarget[2];
            Vec3f lateral;
            lateral[0] = wishdir[0] - dot * toTarget[0];
            lateral[1] = 0.0f;
            lateral[2] = wishdir[2] - dot * toTarget[2];

            f32 latLen = sqrtf(lateral[0] * lateral[0] + lateral[2] * lateral[2]);
            if (latLen > 0.001f) {
                lateral[0] /= latLen;
                lateral[2] /= latLen;

                f32 strafeAccel = wishspeed * TF_GRAPPLE_STRAFE_SCALE * dt;
                m->vel[0] += lateral[0] * strafeAccel;
                m->vel[2] += lateral[2] * strafeAccel;
            }
        }
    }

    /* ── Speed cap ──────────────────────────────────────── */
    {
        f32 speed = sqrtf(m->vel[0] * m->vel[0]
                        + m->vel[1] * m->vel[1]
                        + m->vel[2] * m->vel[2]);
        if (speed > TF_GRAPPLE_SPEED_CAP) {
            f32 scale = TF_GRAPPLE_SPEED_CAP / speed;
            m->vel[0] *= scale;
            m->vel[1] *= scale;
            m->vel[2] *= scale;
        }
    }

    /* Shrink rope length as we get closer (prevents rubber-banding) */
    if (dist < gr->ropeLength) {
        gr->ropeLength = dist;
    }
}

/* ── Detach grapple ────────────────────────────────────────────────── */

void tf_grapple_detach(struct MarioState *m, struct GrappleState *gr) {
    if (!gr->active) return;

    /* Speed boost on release */
    m->vel[0] *= TF_GRAPPLE_DETACH_BOOST;
    m->vel[1] *= TF_GRAPPLE_DETACH_BOOST;
    m->vel[2] *= TF_GRAPPLE_DETACH_BOOST;

    gr->active = 0;
    gr->timer = 0;
    gr->cooldown = TF_GRAPPLE_COOLDOWN;

    m->particleFlags |= PARTICLE_SPARKLES;
    play_sound(SOUND_ACTION_TERRAIN_LANDING, m->marioObj->header.gfx.cameraToObject);
}
