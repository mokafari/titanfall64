/*
 * tf_mantle.c — Titanfall64 mantle/vault system
 *
 * When airborne and touching a wall, probe for a ledge above.
 * If found within height range, interpolate Mario to the top over
 * TF_MANTLE_DURATION frames using an ease-out curve with an arc boost.
 * Preserves most horizontal speed through the mantle.
 */

#include <math.h>
#include "sm64.h"
#include "engine/math_util.h"
#include "game/mario.h"
#include "engine/surface_collision.h"
#include "game/titanfall/tf_movement.h"
#include "game/titanfall/tf_math.h"
#include "game/titanfall/tf_mantle.h"
#include "sounds.h"
#include "audio/external.h"

/*
 * tf_try_mantle — check for a ledge above and initiate mantle
 *
 * Called each airborne frame when Mario is touching a wall and not
 * already mantling or wallrunning. Returns 1 if mantle was started.
 */
s32 tf_try_mantle(struct MarioState *m, struct MantleState *mt) {
    struct Surface *ledgeFloor = NULL;
    struct Surface *ceil = NULL;
    f32 ledgeY;
    f32 ceilY;
    f32 heightAbove;
    f32 wnx, wnz;
    f32 probeX, probeY, probeZ;

    /* Cooldown still active */
    if (mt->cooldown > 0) {
        mt->cooldown--;
        return 0;
    }

    /* Need a wall contact */
    if (m->wall == NULL) {
        return 0;
    }

    /* Wall normal — points away from wall surface */
    wnx = m->wall->normal.x;
    wnz = m->wall->normal.z;

    /*
     * Probe point: start at Mario's position, go forward past the wall
     * surface by TF_MANTLE_FORWARD_CHECK units (opposite of wall normal,
     * i.e. INTO the wall), and up by TF_MANTLE_CHECK_HEIGHT.
     */
    probeX = m->pos[0] - wnx * TF_MANTLE_FORWARD_CHECK;
    probeY = m->pos[1] + TF_MANTLE_CHECK_HEIGHT;
    probeZ = m->pos[2] - wnz * TF_MANTLE_FORWARD_CHECK;

    /* Find the floor at the probe point (this is the ledge top) */
    ledgeY = find_floor(probeX, probeY, probeZ, &ledgeFloor);

    if (ledgeFloor == NULL) {
        return 0;
    }

    /* Height of ledge above Mario's current position */
    heightAbove = ledgeY - m->pos[1];

    /* Must be within valid mantle range */
    if (heightAbove < TF_MANTLE_MIN_HEIGHT || heightAbove > TF_MANTLE_MAX_HEIGHT) {
        return 0;
    }

    /* Check there is enough ceiling clearance at the target */
    ceilY = find_ceil(probeX, ledgeY + 10.0f, probeZ, &ceil);
    if (ceil != NULL && (ceilY - ledgeY) < 160.0f) {
        return 0;  /* not enough headroom */
    }

    /* ── Begin mantle ──────────────────────────────────────────── */
    mt->active = 1;
    mt->timer = 0;

    /* Store start position */
    mt->startPos[0] = m->pos[0];
    mt->startPos[1] = m->pos[1];
    mt->startPos[2] = m->pos[2];

    /*
     * Target position: on top of the ledge, slightly back from the
     * edge (along wall normal) so Mario lands on solid ground.
     */
    mt->targetPos[0] = probeX;
    mt->targetPos[1] = ledgeY;
    mt->targetPos[2] = probeZ;

    /* Preserve horizontal velocity for exit */
    mt->preservedSpeedX = m->vel[0] * TF_MANTLE_SPEED_PRESERVE;
    mt->preservedSpeedZ = m->vel[2] * TF_MANTLE_SPEED_PRESERVE;

    /* Zero velocity during mantle — position is interpolated */
    m->vel[0] = 0.0f;
    m->vel[1] = 0.0f;
    m->vel[2] = 0.0f;
    m->forwardVel = 0.0f;

    play_sound(SOUND_ACTION_TERRAIN_LANDING, m->marioObj->header.gfx.cameraToObject);

    return 1;
}

/*
 * tf_update_mantle — per-frame interpolation while mantle is active
 *
 * Interpolates Mario from startPos to targetPos using ease-out,
 * with a sinusoidal arc boost on Y for a natural vault feel.
 * On completion, restores horizontal velocity and adds a small
 * upward pop.
 */
void tf_update_mantle(struct MarioState *m, struct MantleState *mt) {
    f32 t;
    f32 eased;
    f32 arcBoost;

    if (!mt->active) {
        return;
    }

    mt->timer++;
    t = (f32)mt->timer / (f32)TF_MANTLE_DURATION;

    if (t > 1.0f) {
        t = 1.0f;
    }

    /* Ease-out: 1 - (1 - t)^2 */
    eased = 1.0f - (1.0f - t) * (1.0f - t);

    /* Sinusoidal arc boost — peaks at midpoint, zero at start/end */
    arcBoost = sinf(t * 3.14159f) * 30.0f;

    /* Interpolate position */
    m->pos[0] = mt->startPos[0] + (mt->targetPos[0] - mt->startPos[0]) * eased;
    m->pos[1] = mt->startPos[1] + (mt->targetPos[1] - mt->startPos[1]) * eased + arcBoost;
    m->pos[2] = mt->startPos[2] + (mt->targetPos[2] - mt->startPos[2]) * eased;

    /* Keep velocity zeroed during interpolation */
    m->vel[0] = 0.0f;
    m->vel[1] = 0.0f;
    m->vel[2] = 0.0f;
    m->forwardVel = 0.0f;

    /* Mantle complete */
    if (mt->timer >= TF_MANTLE_DURATION) {
        /* Snap to target */
        m->pos[0] = mt->targetPos[0];
        m->pos[1] = mt->targetPos[1];
        m->pos[2] = mt->targetPos[2];

        /* Restore preserved horizontal speed */
        m->vel[0] = mt->preservedSpeedX;
        m->vel[2] = mt->preservedSpeedZ;

        /* Small upward pop to clear the ledge */
        m->vel[1] = TF_MANTLE_VERTICAL_BOOST;

        /* Sync SM64 internal speed state */
        m->forwardVel = vec3f_magnitude_xz(m->vel);
        if (m->forwardVel > 1.0f) {
            m->faceAngle[1] = atan2s(m->vel[2], m->vel[0]);
        }
        m->slideVelX = m->vel[0];
        m->slideVelZ = m->vel[2];

        /* Reset mantle state, start cooldown */
        mt->active = 0;
        mt->timer = 0;
        mt->cooldown = TF_MANTLE_COOLDOWN;

        m->particleFlags |= PARTICLE_DUST;
    }
}
