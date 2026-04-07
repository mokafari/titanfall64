/*
 * tf_wallrun.c — Titanfall 2 wallrunning (research-calibrated)
 *
 * Based on community reverse-engineering and Respawn GDC talks:
 * - Approach: 20-75° from wall normal (dot 0.2 to 0.85)
 * - Duration: ~1.75s, gravity starts ramping at 0.5s
 * - Gravity: 12.5% for first 0.5s, ramps to 100% over next 0.75s
 * - Kick: fixed 300 units normal + 280 up + 85% preserved along-wall
 * - Chains: different wall (60°+ angle diff) = instant, same wall = needs ground
 * - Stick force: ~200 units/s² into wall (6.67/frame at 30fps)
 * - Camera roll: 12° lean
 * - Speed decay: ~5-10%/second
 */

#include <math.h>
#include "sm64.h"
#include "game/mario.h"
#include "game/titanfall/tf_movement.h"
#include "game/titanfall/tf_math.h"
#include "sounds.h"
#include "audio/external.h"

/* ── Research-calibrated constants ───────────────────────────────── */

/* Approach angle window: dot(velDir, -wallNormal) */
#define WR_APPROACH_MIN     0.10f   /* cos(84°) — nearly parallel      */
#define WR_APPROACH_MAX     0.92f   /* cos(23°) — steep approach        */

/* Duration & gravity timing (in frames at 30fps) */
#define WR_MAX_FRAMES       52      /* ~1.75s total                     */
#define WR_GRAVITY_RAMP_START 15    /* ~0.5s: gravity begins ramping    */
#define WR_GRAVITY_RAMP_END  38     /* ~1.25s: gravity reaches 100%     */
#define WR_GRAVITY_MIN      0.125f  /* 12.5% gravity while attached     */

/* Wall stick: 200 units/s² = 6.67 units/frame at 30fps */
#define WR_STICK_FORCE      6.67f

/* Wall-kick (additive, TF2 style)
 * Total kick = normal * KICK_NORMAL + up * KICK_UP + runDir * speed * KICK_PRESERVE
 * All values in units/frame (SM64 velocity scale)
 */
/*
 * SM64 velocity is units/frame, not units/sec like Source.
 * Mario runs at ~32-48 units/frame. We need the normal kick to be
 * significant relative to that — at least 60-70% of run speed
 * to get a ~40° exit angle from the wall.
 */
#define WR_KICK_NORMAL      35.0f   /* strong outward impulse           */
#define WR_KICK_UP          35.0f   /* strong upward impulse            */
#define WR_KICK_PRESERVE     0.70f  /* fraction of along-wall speed kept*/
#define WR_KICK_BONUS        1.08f  /* 8% total speed boost per kick    */

/* Speed decay: ~7%/second ≈ 0.997^frame */
#define WR_SPEED_DECAY      0.9977f

/* Camera */
#define WR_CAM_ROLL         12.0f   /* degrees of camera lean           */

/* Chain: minimum angle between wall normals for instant reattach (cos 60°) */
#define WR_CHAIN_MIN_DOT    0.5f    /* walls must differ by 60°+        */

/* Same-wall cooldown */
#define WR_SAME_WALL_CD     15      /* frames (~0.5s)                   */

/* Minimum speed */
#define WR_MIN_RUN_SPEED    10.0f

/* ── Helper: check if two walls are "different enough" to chain ─── */

static s32 walls_differ_enough(struct WallrunState *wr, struct Surface *newWall) {
    if (newWall == NULL) return 0;
    if (wr->lastWall == NULL) return 1;
    if (newWall == wr->lastWall) return 0;

    /* Dot product of old and new wall normals */
    f32 dot = wr->wallNormal[0] * newWall->normal.x
            + wr->wallNormal[2] * newWall->normal.z;

    /* cos(60°) = 0.5 → dot < 0.5 means walls differ by 60°+ */
    return (dot < WR_CHAIN_MIN_DOT);
}

/* ── Attach ─────────────────────────────────────────────────────── */

s32 tf_try_wallrun_attach(struct MarioState *m, struct WallrunState *wr) {
    if (m->wall == NULL) return 0;

    /* Cooldown logic: same wall blocked, different wall checks angle */
    if (wr->cooldown > 0) {
        if (walls_differ_enough(wr, m->wall)) {
            /* Different wall with 60°+ angle: allow instant chain */
        } else {
            wr->cooldown--;
            if (wr->cooldown == 0) wr->lastWall = NULL;
            return 0;
        }
    }

    f32 hspeed = vec3f_magnitude_xz(m->vel);
    if (hspeed < TF_CVAR_F("WR.MinSpeed", TF_WR_MIN_SPEED)) return 0;

    f32 height = m->pos[1] - m->floorHeight;
    if (height < TF_CVAR_F("WR.MinHeight", TF_WR_MIN_HEIGHT)) return 0;

    /* Wall normal */
    f32 wnx = m->wall->normal.x;
    f32 wnz = m->wall->normal.z;
    f32 wnLen = sqrtf(wnx * wnx + wnz * wnz);
    if (wnLen < 0.01f) return 0;
    wnx /= wnLen;
    wnz /= wnLen;

    /* Approach angle */
    f32 vdx = m->vel[0] / hspeed;
    f32 vdz = m->vel[2] / hspeed;
    f32 approach = -(vdx * wnx + vdz * wnz);
    if (approach < TF_CVAR_F("WR.ApproachMin", WR_APPROACH_MIN) || approach > TF_CVAR_F("WR.ApproachMax", WR_APPROACH_MAX)) return 0;

    /* Side detection */
    f32 crossY = vdx * wnz - vdz * wnx;
    wr->side = (crossY > 0) ? 0 : 1;

    /* Run direction: project velocity onto wall plane */
    f32 dot = vdx * wnx + vdz * wnz;
    f32 rdx = vdx - dot * wnx;
    f32 rdz = vdz - dot * wnz;
    f32 rdLen = sqrtf(rdx * rdx + rdz * rdz);
    if (rdLen < 0.01f) return 0;
    rdx /= rdLen;
    rdz /= rdLen;

    /* Commit */
    wr->wallNormal[0] = wnx;
    wr->wallNormal[1] = 0.0f;
    wr->wallNormal[2] = wnz;
    wr->runDir[0] = rdx;
    wr->runDir[1] = 0.0f;
    wr->runDir[2] = rdz;
    wr->entrySpeed = hspeed;
    wr->timer = 0;
    wr->active = 1;
    wr->lastWall = m->wall;

    /* Snap velocity to wall direction + upward bump */
    m->vel[0] = rdx * wr->entrySpeed;
    m->vel[2] = rdz * wr->entrySpeed;
    m->vel[1] = TF_CVAR_F("WR.EntryUpkick", TF_WR_ENTRY_UPKICK);

    play_sound(SOUND_ACTION_TERRAIN_LANDING, m->marioObj->header.gfx.cameraToObject);
    return 1;
}

/* ── Per-frame update ───────────────────────────────────────────── */

void tf_update_wallrun(struct MarioState *m, struct WallrunState *wr, f32 dt) {
    wr->timer++;
    (void)dt;

    if (wr->timer > TF_CVAR_I("WR.MaxFrames", WR_MAX_FRAMES)) {
        tf_wallrun_detach(m, wr);
        return;
    }

    /* Lost wall after initial grace period */
    if (m->wall == NULL && wr->timer > 4) {
        tf_wallrun_detach(m, wr);
        return;
    }

    /* Suppress vanilla gravity */
    m->action = ACT_FLYING;

    /* ── Gravity: 12.5% base, ramps to 100% ──────────── */
    f32 gravMult;
    if (wr->timer < WR_GRAVITY_RAMP_START) {
        gravMult = WR_GRAVITY_MIN;
    } else if (wr->timer < WR_GRAVITY_RAMP_END) {
        f32 t = (f32)(wr->timer - WR_GRAVITY_RAMP_START)
              / (f32)(WR_GRAVITY_RAMP_END - WR_GRAVITY_RAMP_START);
        gravMult = WR_GRAVITY_MIN + (1.0f - WR_GRAVITY_MIN) * t;
    } else {
        gravMult = 1.0f;
    }
    m->vel[1] -= 4.0f * gravMult;
    if (m->vel[1] < -25.0f) m->vel[1] = -25.0f;

    /* Upward snap at very start */
    if (wr->timer <= 3 && m->vel[1] < 6.0f) {
        m->vel[1] = 6.0f;
    }

    /* ── Speed along wall with gentle decay ───────────── */
    f32 currentSpeed = wr->entrySpeed * powf(WR_SPEED_DECAY, (f32)wr->timer);
    if (currentSpeed < WR_MIN_RUN_SPEED) {
        tf_wallrun_detach(m, wr);
        return;
    }

    m->vel[0] = wr->runDir[0] * currentSpeed;
    m->vel[2] = wr->runDir[2] * currentSpeed;

    /* ── Stick force: push into wall ──────────────────── */
    m->vel[0] -= wr->wallNormal[0] * WR_STICK_FORCE;
    m->vel[2] -= wr->wallNormal[2] * WR_STICK_FORCE;

    /* ── Input: lean away = detach ────────────────────── */
    Vec3f wishdir;
    f32 wishspeed;
    get_wishdir(m, wishdir, &wishspeed);
    if (wishspeed > 0.1f) {
        f32 lean = wishdir[0] * wr->wallNormal[0] + wishdir[2] * wr->wallNormal[2];
        if (lean > 0.5f) {
            tf_wallrun_detach(m, wr);
            return;
        }
    }

    /* ── Wall-kick ────────────────────────────────────── */
    if (m->input & INPUT_A_PRESSED) {
        tf_wallrun_jump(m, wr);
        return;
    }

    /* ── Camera ───────────────────────────────────────── */
    /* Camera leans toward the wall: side 0 = wall on left → lean left (try both signs) */
    f32 camRoll = TF_CVAR_F("Cam.WallrunRoll", WR_CAM_ROLL);
    gTFState.camera.targetRoll = (wr->side == 0) ? camRoll : -camRoll;
}

/* ── Wall-kick: TF2 additive formula ────────────────────────────── */

void tf_wallrun_jump(struct MarioState *m, struct WallrunState *wr) {
    /*
     * Titanfall 2 wall-kick (from community reverse-engineering):
     *
     * kick = wallNormal * KICK_NORMAL    (fixed outward impulse)
     *      + up         * KICK_UP        (fixed upward impulse)
     *      + runDir     * speed * PRESERVE (preserved along-wall speed)
     *
     * Total speed gets an 8% bonus to reward chains.
     */
    f32 currentSpeed = wr->entrySpeed * powf(WR_SPEED_DECAY, (f32)wr->timer);
    f32 preservedSpeed = currentSpeed * TF_CVAR_F("WR.KickPreserve", WR_KICK_PRESERVE);
    f32 kickNormal = TF_CVAR_F("WR.KickNormal", WR_KICK_NORMAL);
    f32 kickUp = TF_CVAR_F("WR.KickUp", WR_KICK_UP);
    f32 kickBonus = TF_CVAR_F("WR.KickBonus", WR_KICK_BONUS);

    /* Build kick velocity: additive components */
    m->vel[0] = wr->wallNormal[0] * kickNormal
              + wr->runDir[0] * preservedSpeed;
    m->vel[2] = wr->wallNormal[2] * kickNormal
              + wr->runDir[2] * preservedSpeed;
    m->vel[1] = kickUp;

    /* Apply chain speed bonus */
    m->vel[0] *= kickBonus;
    m->vel[2] *= kickBonus;

    wr->active = 0;
    wr->cooldown = WR_SAME_WALL_CD;
    gTFState.camera.targetRoll = 0.0f;
    gTFState.canDoubleJump = 1;

    m->particleFlags |= PARTICLE_HORIZONTAL_STAR | PARTICLE_DUST;
    play_sound(SOUND_ACTION_TERRAIN_JUMP, m->marioObj->header.gfx.cameraToObject);
    m->action = ACT_FREEFALL;

    /* Wall-kick animation timer (used by tf_movement.c for SLIDEJUMP anim) */
    gTFState.wallKickTimer = 12;
}

/* ── Wall kick (not wallrunning — just touching a wall in air) ──── */

s32 tf_wall_kick(struct MarioState *m) {
    if (m->wall == NULL) return 0;

    f32 wnx = m->wall->normal.x;
    f32 wnz = m->wall->normal.z;
    f32 wnLen = sqrtf(wnx * wnx + wnz * wnz);
    if (wnLen < 0.01f) return 0;
    wnx /= wnLen;
    wnz /= wnLen;

    f32 hspeed = vec3f_magnitude_xz(m->vel);
    f32 keepSpeed = tf_fmaxf(hspeed * 0.5f, 15.0f);

    /* Bounce off wall normal + keep some forward momentum */
    f32 vdx = (hspeed > 1.0f) ? m->vel[0] / hspeed : 0.0f;
    f32 vdz = (hspeed > 1.0f) ? m->vel[2] / hspeed : 0.0f;

    /* Project velocity onto wall plane for run direction */
    f32 dot = vdx * wnx + vdz * wnz;
    f32 rdx = vdx - dot * wnx;
    f32 rdz = vdz - dot * wnz;
    f32 rdLen = sqrtf(rdx * rdx + rdz * rdz);
    if (rdLen > 0.01f) { rdx /= rdLen; rdz /= rdLen; }

    f32 wkNormal = TF_CVAR_F("WK.Normal", 30.0f);
    f32 wkUp = TF_CVAR_F("WK.Up", 45.0f);
    m->vel[0] = wnx * wkNormal + rdx * keepSpeed;
    m->vel[2] = wnz * wkNormal + rdz * keepSpeed;
    m->vel[1] = wkUp;

    m->action = ACT_FREEFALL;
    gTFState.canDoubleJump = 1;
    gTFState.wallKickTimer = 12;

    m->particleFlags |= PARTICLE_HORIZONTAL_STAR;
    play_sound(SOUND_ACTION_TERRAIN_JUMP, m->marioObj->header.gfx.cameraToObject);
    return 1;
}

/* ── Detach ─────────────────────────────────────────────────────── */

void tf_wallrun_detach(struct MarioState *m, struct WallrunState *wr) {
    wr->active = 0;
    wr->cooldown = WR_SAME_WALL_CD;
    gTFState.camera.targetRoll = 0.0f;
    m->action = ACT_FREEFALL;
}
