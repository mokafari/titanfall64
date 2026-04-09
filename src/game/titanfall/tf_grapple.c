/*
 * tf_grapple.c — Titanfall64 grapple/hook system (revised)
 *
 * R button fires a ray from camera. If geometry is hit within range,
 * Mario is pulled toward the target with pendulum-style swing physics.
 *
 * Visual: spawns small flame objects along the "rope" each frame.
 *
 * Key fixes from v1:
 * - Pull force scales with distance (stronger far away, gentle close up)
 * - Gravity counteraction is simpler and more consistent
 * - Action state properly set to ACT_FREEFALL during grapple
 * - Rope visuals via particle trail
 * - Better detach: A button also detaches (for jump-cancel)
 */

#include <math.h>
#include "sm64.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "game/mario.h"
#include "game/object_helpers.h"
#include "game/titanfall/tf_movement.h"
#include "game/titanfall/tf_math.h"
#include "game/titanfall/tf_grapple.h"
#include "behavior_data.h"
#include "model_ids.h"
#include "sounds.h"
#include "audio/external.h"

/* ── Rope visuals ──────────────────────────────────────────────── */

#define GRAPPLE_ROPE_SEGMENTS  6
static struct Object *sRopeObjs[GRAPPLE_ROPE_SEGMENTS] = { 0 };

static void tf_grapple_spawn_rope(struct MarioState *m, struct GrappleState *gr) {
    Vec3f start = { m->pos[0], m->pos[1] + 100.0f, m->pos[2] };

    for (s32 i = 0; i < GRAPPLE_ROPE_SEGMENTS; i++) {
        f32 t = (f32)(i + 1) / (f32)(GRAPPLE_ROPE_SEGMENTS + 1);

        /* Slight sag in the middle of the rope */
        f32 sag = sinf(t * 3.14159f) * -20.0f;

        f32 x = start[0] + (gr->targetPos[0] - start[0]) * t;
        f32 y = start[1] + (gr->targetPos[1] - start[1]) * t + sag;
        f32 z = start[2] + (gr->targetPos[2] - start[2]) * t;

        struct Object *obj = sRopeObjs[i];
        if (obj == NULL || obj->activeFlags == 0) {
            obj = spawn_object(m->marioObj, MODEL_BLUE_FLAME, bhvFlame);
            if (obj == NULL) continue;
            obj->oInteractType = 0;  /* don't burn Mario */
            sRopeObjs[i] = obj;
        }

        obj->oPosX = x;
        obj->oPosY = y;
        obj->oPosZ = z;
        obj->header.gfx.scale[0] = 1.5f;
        obj->header.gfx.scale[1] = 1.5f;
        obj->header.gfx.scale[2] = 1.5f;
    }
}

static void tf_grapple_destroy_rope(void) {
    for (s32 i = 0; i < GRAPPLE_ROPE_SEGMENTS; i++) {
        if (sRopeObjs[i] != NULL && sRopeObjs[i]->activeFlags != 0) {
            obj_mark_for_deletion(sRopeObjs[i]);
        }
        sRopeObjs[i] = NULL;
    }
}

/* ── Step-based raycast ────────────────────────────────────────── */

static s32 tf_raycast_step(Vec3f origin, Vec3f dir, f32 maxDist, Vec3f hitPos) {
    f32 step = TF_GRAPPLE_RAY_STEP;
    s32 numSteps = (s32)(maxDist / step);
    if (numSteps < 1) numSteps = 1;
    if (numSteps > 200) numSteps = 200;

    for (s32 i = 1; i <= numSteps; i++) {
        f32 t = step * (f32)i;
        if (t > maxDist) t = maxDist;

        f32 px = origin[0] + dir[0] * t;
        f32 py = origin[1] + dir[1] * t;
        f32 pz = origin[2] + dir[2] * t;

        /* Floor: ray passes below it */
        struct Surface *floor = NULL;
        f32 floorY = find_floor(px, py + 80.0f, pz, &floor);
        if (floor != NULL && py < floorY + 5.0f) {
            hitPos[0] = px;
            hitPos[1] = floorY;
            hitPos[2] = pz;
            return 1;
        }

        /* Ceiling: ray passes above it */
        struct Surface *ceil = NULL;
        f32 ceilY = find_ceil(px, py - 10.0f, pz, &ceil);
        if (ceil != NULL && ceilY > -10000.0f && py > ceilY - 5.0f) {
            hitPos[0] = px;
            hitPos[1] = ceilY;
            hitPos[2] = pz;
            return 1;
        }

        /* Wall collision */
        struct WallCollisionData wd;
        wd.x = px;
        wd.y = py;
        wd.z = pz;
        wd.offsetY = 30.0f;
        wd.radius = 40.0f;
        wd.numWalls = 0;
        if (find_wall_collisions(&wd) != 0 && wd.numWalls > 0) {
            hitPos[0] = wd.x;
            hitPos[1] = py;
            hitPos[2] = wd.z;
            return 1;
        }
    }

    return 0;
}

/* ── Fire grapple ──────────────────────────────────────────────── */

s32 tf_try_grapple(struct MarioState *m, struct GrappleState *gr) {
    if (gr->active) return 0;
    if (gr->cooldown > 0) return 0;
    if (!(m->controller->buttonPressed & R_TRIG)) return 0;

    /* Camera-forward direction */
    s16 yawS16   = (s16)(gTFState.camera.yaw   / 360.0f * 65536.0f);
    s16 pitchS16 = (s16)(gTFState.camera.pitch / 360.0f * 65536.0f);

    Vec3f dir;
    dir[0] =  coss(pitchS16) * sins(yawS16);
    dir[1] = -sins(pitchS16);
    dir[2] =  coss(pitchS16) * coss(yawS16);

    Vec3f origin = { m->pos[0], m->pos[1] + 120.0f, m->pos[2] };

    Vec3f hitPos;
    if (!tf_raycast_step(origin, dir, TF_GRAPPLE_MAX_RANGE, hitPos)) {
        return 0;
    }

    /* Don't grapple to a point directly below (would slam into ground) */
    if (hitPos[1] < m->pos[1] - 50.0f) {
        return 0;
    }

    gr->active = 1;
    gr->timer = 0;
    vec3f_copy(gr->targetPos, hitPos);

    f32 dx = hitPos[0] - m->pos[0];
    f32 dy = hitPos[1] - m->pos[1];
    f32 dz = hitPos[2] - m->pos[2];
    gr->ropeLength = sqrtf(dx * dx + dy * dy + dz * dz);

    /* Give initial pull impulse toward target */
    f32 dist = gr->ropeLength;
    if (dist > 1.0f) {
        m->vel[0] += (dx / dist) * 8.0f;
        m->vel[1] += (dy / dist) * 8.0f + 5.0f;
        m->vel[2] += (dz / dist) * 8.0f;
    }

    play_sound(SOUND_ACTION_TERRAIN_JUMP, m->marioObj->header.gfx.cameraToObject);
    m->particleFlags |= PARTICLE_HORIZONTAL_STAR;

    return 1;
}

/* ── Per-frame update ──────────────────────────────────────────── */

void tf_update_grapple(struct MarioState *m, struct GrappleState *gr, f32 dt) {
    if (!gr->active) return;
    (void)dt;

    gr->timer++;

    /* Keep in air */
    m->action = ACT_FREEFALL;

    /* Direction + distance to target */
    f32 dx = gr->targetPos[0] - m->pos[0];
    f32 dy = gr->targetPos[1] - m->pos[1];
    f32 dz = gr->targetPos[2] - m->pos[2];
    f32 dist = sqrtf(dx * dx + dy * dy + dz * dz);

    /* Detach conditions */
    if (gr->timer > TF_GRAPPLE_MAX_DURATION) { tf_grapple_detach(m, gr); return; }
    if (dist < TF_GRAPPLE_MIN_DIST)          { tf_grapple_detach(m, gr); return; }
    if (!(m->controller->buttonDown & R_TRIG)){ tf_grapple_detach(m, gr); return; }
    if (m->input & INPUT_A_PRESSED)           { tf_grapple_detach(m, gr); return; } /* jump-cancel */

    /* Normalized rope direction (toward target) */
    f32 invDist = 1.0f / dist;
    f32 ndx = dx * invDist;
    f32 ndy = dy * invDist;
    f32 ndz = dz * invDist;

    /*
     * ── Pendulum swing physics ──────────────────────────
     *
     * The grapple acts like a rope, not a jetpack.
     *
     * 1. Apply gravity normally (let SM64's apply_gravity handle it,
     *    but counteract a small portion for floatiness).
     * 2. Slowly reel in the rope each frame.
     * 3. If Mario is beyond the rope length, enforce the constraint:
     *    remove the component of velocity moving AWAY from the target.
     *    This is what creates the pendulum arc — velocity perpendicular
     *    to the rope is preserved, velocity along the rope (outward) is killed.
     * 4. A small inward pull keeps tension on the rope.
     */

    /* Reel in: shorten the rope over time (pull yourself closer) */
    f32 reelSpeed = 3.0f;  /* units/frame rope shortening */
    gr->ropeLength -= reelSpeed;
    if (gr->ropeLength < TF_GRAPPLE_MIN_DIST) {
        gr->ropeLength = TF_GRAPPLE_MIN_DIST;
    }

    /* Reduced gravity: counteract 60% of SM64's ~4.0/frame gravity */
    m->vel[1] += 4.0f * 0.6f;

    /* Small constant inward pull (tension) */
    f32 tension = 4.0f;
    m->vel[0] += ndx * tension;
    m->vel[1] += (ndy + 0.15f) * tension;  /* slight upward bias */
    m->vel[2] += ndz * tension;

    /* ── Rope constraint: enforce max rope length ──────── */
    if (dist > gr->ropeLength) {
        /*
         * Project velocity onto rope direction.
         * If the component along the rope is outward (negative dot),
         * remove it. This is what creates the swing arc.
         */
        f32 velDotRope = m->vel[0] * ndx + m->vel[1] * ndy + m->vel[2] * ndz;

        if (velDotRope < 0.0f) {
            /* Moving away from target — remove that component */
            m->vel[0] -= velDotRope * ndx;
            m->vel[1] -= velDotRope * ndy;
            m->vel[2] -= velDotRope * ndz;
        }

        /* Snap position back to rope length */
        f32 overshoot = dist - gr->ropeLength;
        m->pos[0] += ndx * overshoot;
        m->pos[1] += ndy * overshoot;
        m->pos[2] += ndz * overshoot;
    }

    /* ── Air strafing (lateral control during swing) ───── */
    Vec3f wishdir;
    f32 wishspeed;
    get_wishdir(m, wishdir, &wishspeed);
    if (wishspeed > 0.01f) {
        /* Project wish onto plane perpendicular to rope */
        f32 dot = wishdir[0] * ndx + wishdir[1] * ndy + wishdir[2] * ndz;
        f32 lx = wishdir[0] - dot * ndx;
        f32 ly = wishdir[1] - dot * ndy;
        f32 lz = wishdir[2] - dot * ndz;
        f32 ll = sqrtf(lx * lx + ly * ly + lz * lz);
        if (ll > 0.001f) {
            lx /= ll; ly /= ll; lz /= ll;
            /* Stronger strafing than before — makes swinging feel responsive */
            f32 strafeAccel = 2.5f;
            m->vel[0] += lx * strafeAccel;
            m->vel[1] += ly * strafeAccel;
            m->vel[2] += lz * strafeAccel;
        }
    }

    /* Speed cap */
    f32 speed = sqrtf(m->vel[0] * m->vel[0] + m->vel[1] * m->vel[1] + m->vel[2] * m->vel[2]);
    if (speed > TF_GRAPPLE_SPEED_CAP) {
        f32 scale = TF_GRAPPLE_SPEED_CAP / speed;
        m->vel[0] *= scale;
        m->vel[1] *= scale;
        m->vel[2] *= scale;
    }

    /* Track shrinking rope */
    if (dist < gr->ropeLength) {
        gr->ropeLength = dist;
    }

    /* Spawn rope visuals */
    tf_grapple_spawn_rope(m, gr);

    /* Particles at the grapple point (subtle anchor indicator) */
    if ((gr->timer & 3) == 0) {
        m->particleFlags |= PARTICLE_SPARKLES;
    }
}

/* ── Detach ────────────────────────────────────────────────────── */

void tf_grapple_detach(struct MarioState *m, struct GrappleState *gr) {
    if (!gr->active) return;

    /* Speed boost on release */
    m->vel[0] *= TF_GRAPPLE_DETACH_BOOST;
    m->vel[2] *= TF_GRAPPLE_DETACH_BOOST;
    /* Don't boost vertical — prevents launch-to-death */

    /* Re-enable double jump after grapple */
    gTFState.canDoubleJump = 1;

    gr->active = 0;
    gr->timer = 0;
    gr->cooldown = TF_GRAPPLE_COOLDOWN;

    tf_grapple_destroy_rope();

    m->particleFlags |= PARTICLE_SPARKLES | PARTICLE_HORIZONTAL_STAR;
    play_sound(SOUND_ACTION_TERRAIN_LANDING, m->marioObj->header.gfx.cameraToObject);
}
