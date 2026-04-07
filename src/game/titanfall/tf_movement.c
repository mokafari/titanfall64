/*
 * tf_movement.c — Titanfall64 main movement loop
 *
 * Movement chain: Sprint → Slide → Slide-Jump → Air Strafe → Land → ...
 * Attacks: B = dive kick (ground/air), Z in air = ground pound
 * Wallrun: approach wall at angle, A to kick off, chains build speed
 */

#include <string.h>
#include <math.h>
#include "sm64.h"
#include "engine/math_util.h"
#include "game/mario.h"
#include "game/mario_step.h"
#include "engine/surface_collision.h"
#include "game/interaction.h"
#include "game/titanfall/tf_movement.h"
#include "game/titanfall/tf_math.h"
#include "mario_animation_ids.h"
#include "sounds.h"
#include "audio/external.h"

struct TFPlayerState gTFState = { 0 };

/* Track previous frame state for transition detection */
static u8 sWasAirborne = 0;
/* sJustWallKicked removed — using gTFState.wallKickTimer instead */

/* Attack state */
static u8 sDiving = 0;
static u8 sGroundPounding = 0;
static s16 sGroundPoundTimer = 0;

void tf_movement_init(void) {
    memset(&gTFState, 0, sizeof(gTFState));
    gTFState.camera.distance = TF_CAM_DISTANCE;
    sWasAirborne = 0;
    gTFState.wallKickTimer = 0;
    sDiving = 0;
    sGroundPounding = 0;
    sGroundPoundTimer = 0;
}

static void tf_sync_vel_to_mario(struct MarioState *m) {
    f32 hspeed = vec3f_magnitude_xz(m->vel);
    m->forwardVel = hspeed;
    if (hspeed > 1.0f) {
        m->faceAngle[1] = atan2s(m->vel[2], m->vel[0]);
    }
    m->slideVelX = m->vel[0];
    m->slideVelZ = m->vel[2];
}

/* ── Jump with speed boost ──────────────────────────────────────── */

static void tf_do_jump(struct MarioState *m, f32 jumpVel, f32 hBoost) {
    if (hBoost > 1.0f) {
        m->vel[0] *= hBoost;
        m->vel[2] *= hBoost;
    }
    m->vel[1] = jumpVel;
    gTFState.jumpGraceTimer = 0;
    gTFState.jumpBufferTimer = 0;
    gTFState.canDoubleJump = 1;
    sDiving = 0;
    sGroundPounding = 0;
    m->particleFlags |= PARTICLE_DUST;
    play_sound(SOUND_ACTION_TERRAIN_JUMP, m->marioObj->header.gfx.cameraToObject);
}

/* ── Instant slide entry ────────────────────────────────────────── */

static void tf_instant_slide(struct MarioState *m) {
    gTFState.slide.active = 1;
    gTFState.slide.timer = 0;
    gTFState.slide.entrySpeed = vec3f_magnitude_xz(m->vel);
    sDiving = 0;
    sGroundPounding = 0;
    m->particleFlags |= PARTICLE_MIST_CIRCLE;
    play_sound(SOUND_ACTION_TERRAIN_LANDING, m->marioObj->header.gfx.cameraToObject);
}

/* ── Dive attack ────────────────────────────────────────────────── */

static void tf_do_dive(struct MarioState *m) {
    s16 camYawS16 = (s16)(gTFState.camera.yaw / 360.0f * 65536.0f);
    f32 diveSpeed = tf_fmaxf(vec3f_magnitude_xz(m->vel), 40.0f);

    /* Launch forward in camera direction */
    m->vel[0] = sins(camYawS16) * diveSpeed * 1.2f;
    m->vel[2] = coss(camYawS16) * diveSpeed * 1.2f;
    m->vel[1] = 15.0f;  /* slight upward arc */

    sDiving = 1;
    sGroundPounding = 0;
    m->action = ACT_DIVE;  /* has ACT_FLAG_ATTACKING — damages enemies */
    m->particleFlags |= PARTICLE_HORIZONTAL_STAR;
    play_sound(SOUND_MARIO_HOOHOO, m->marioObj->header.gfx.cameraToObject);
}

/* ── Ground pound ───────────────────────────────────────────────── */

static void tf_do_ground_pound(struct MarioState *m) {
    m->vel[0] = 0.0f;
    m->vel[2] = 0.0f;
    m->vel[1] = 10.0f;  /* tiny upward pause first */
    sGroundPounding = 1;
    sGroundPoundTimer = 0;
    sDiving = 0;
    m->action = ACT_GROUND_POUND;  /* has ACT_FLAG_ATTACKING */
    play_sound(SOUND_ACTION_TERRAIN_JUMP, m->marioObj->header.gfx.cameraToObject);
}

/* ══════════════════════════════════════════════════════════════════ */

void tf_movement_update(struct MarioState *m) {
    f32 dt = TF_DT;

    if (m == NULL || m->marioObj == NULL || m->controller == NULL) return;
    if (m->floor == NULL) return;

    /* ── Grounded check ──────────────────────────────────── */
    s32 onGround = (m->pos[1] <= m->floorHeight + 4.0f) && (m->vel[1] <= 0.0f);
    u8 justLanded = sWasAirborne && onGround && !gTFState.slide.active;

    /* Coyote time */
    if (onGround) {
        gTFState.jumpGraceTimer = TF_JUMP_GRACE_FRAMES;
        gTFState.lastGroundSpeed = vec3f_magnitude_xz(m->vel);
    } else if (gTFState.jumpGraceTimer > 0) {
        gTFState.jumpGraceTimer--;
    }

    /* Jump buffer */
    if (m->input & INPUT_A_PRESSED) {
        gTFState.jumpBufferTimer = TF_JUMP_BUFFER_FRAMES;
    } else if (gTFState.jumpBufferTimer > 0) {
        gTFState.jumpBufferTimer--;
    }

    /* Wall-kick animation timer */
    if (gTFState.wallKickTimer > 0) gTFState.wallKickTimer--;

    /* ── Ground pound logic (in-progress) ─────────────── */
    if (sGroundPounding) {
        sGroundPoundTimer++;
        if (sGroundPoundTimer < 5) {
            /* Brief pause at top */
            m->vel[0] = 0.0f;
            m->vel[2] = 0.0f;
            m->vel[1] = 2.0f;
        } else {
            /* SLAM down */
            m->vel[1] = -75.0f;
            m->vel[0] = 0.0f;
            m->vel[2] = 0.0f;
        }
        m->action = ACT_GROUND_POUND;
        tf_sync_vel_to_mario(m);
        perform_air_step(m, 0);

        if (onGround && sGroundPoundTimer > 5) {
            /* Landed — shockwave */
            sGroundPounding = 0;
            sGroundPoundTimer = 0;
            m->vel[1] = 0.0f;
            m->particleFlags |= PARTICLE_MIST_CIRCLE | PARTICLE_HORIZONTAL_STAR;
            play_sound(SOUND_ACTION_TERRAIN_LANDING, m->marioObj->header.gfx.cameraToObject);
        }
        goto post_movement;
    }

    /* ── Dive logic (in-progress) ─────────────────────── */
    if (sDiving && !onGround) {
        m->action = ACT_DIVE;
        tf_sync_vel_to_mario(m);
        perform_air_step(m, 0);
        if (onGround || m->pos[1] <= m->floorHeight + 2.0f) {
            /* Dive landing → slide */
            sDiving = 0;
            tf_instant_slide(m);
            m->particleFlags |= PARTICLE_DUST;
        }
        goto post_movement;
    }
    if (sDiving && onGround) {
        /* Hit ground during dive → transition to slide */
        sDiving = 0;
        tf_instant_slide(m);
        m->particleFlags |= PARTICLE_DUST;
    }

    /*
     * ═══════��══ LANDING TRANSITIONS ══════════
     * Z held → instant slide (preserve speed)
     * A held → instant bhop (preserve speed + small boost)
     */
    if (justLanded) {
        m->particleFlags |= PARTICLE_DUST;

        if (m->input & INPUT_Z_DOWN) {
            tf_instant_slide(m);
            gTFState.canDoubleJump = 0;
            gTFState.jumpsAvailable = 2;
        } else if ((m->input & INPUT_A_DOWN) || gTFState.jumpBufferTimer > 0) {
            gTFState.jumpsAvailable = 2;
            tf_do_jump(m, TF_CVAR_F("Jump.Vel", TF_JUMP_VEL) * 0.9f, 1.03f);
            m->action = ACT_FREEFALL;
            tf_sync_vel_to_mario(m);
            perform_air_step(m, 0);
            goto post_movement;
        } else {
            gTFState.jumpsAvailable = 2;
            gTFState.canDoubleJump = 0;
            play_sound(SOUND_ACTION_TERRAIN_LANDING, m->marioObj->header.gfx.cameraToObject);
        }
    }

    /* ══════════ STATE MACHINE ══════════ */

    if (!onGround && !gTFState.slide.active) {
        /* ── AIRBORNE ──────────────────────────────────── */

        /* B in air = dive attack */
        if ((m->input & INPUT_B_PRESSED) && !gTFState.wallrun.active) {
            tf_do_dive(m);
            tf_sync_vel_to_mario(m);
            perform_air_step(m, 0);
            goto post_movement;
        }

        if (gTFState.wallrun.active) {
            tf_update_wallrun(m, &gTFState.wallrun, dt);
        } else {
            m->action = ACT_FREEFALL;
            tf_air_move(m, dt);

            if (m->input & INPUT_A_PRESSED) {
                /* Wall kick: priority over double jump when touching a wall */
                if (m->wall != NULL && tf_wall_kick(m)) {
                    /* wall kick succeeded */
                } else if (gTFState.canDoubleJump) {
                    tf_do_jump(m, TF_CVAR_F("Jump.DoubleVel", TF_DOUBLE_JUMP_VEL), 1.0f);
                    gTFState.canDoubleJump = 0;
                    m->particleFlags |= PARTICLE_SPARKLES;
                }
            }
        }

        tf_sync_vel_to_mario(m);
        perform_air_step(m, 0);

        /* Wallrun check AFTER air step */
        if (!gTFState.wallrun.active && m->wall != NULL) {
            if (m->wall != gTFState.wallrun.lastWall || gTFState.wallrun.cooldown == 0) {
                if (tf_try_wallrun_attach(m, &gTFState.wallrun)) {
                    m->particleFlags |= PARTICLE_HORIZONTAL_STAR;
                }
            }
        }

    } else if (gTFState.slide.active) {
        /* ── SLIDING ───────────────────────────────────── */
        m->action = ACT_CROUCH_SLIDE;
        tf_update_slide(m, &gTFState.slide, dt);

        if (m->floor == NULL || m->pos[1] > m->floorHeight + 10.0f) {
            gTFState.slide.active = 0;
        }

        tf_sync_vel_to_mario(m);
        if (m->floor != NULL) {
            perform_ground_step(m);
        }

    } else {
        /* ── GROUNDED ──────────────────────────────────── */
        m->action = ACT_WALKING;

        /* B on ground: moving = dive, standing = ground pound */
        if (m->input & INPUT_B_PRESSED) {
            if (vec3f_magnitude_xz(m->vel) > 8.0f) {
                tf_do_dive(m);
                tf_sync_vel_to_mario(m);
                perform_air_step(m, 0);
                goto post_movement;
            } else {
                tf_do_ground_pound(m);
                goto post_movement;
            }
        }

        /* Z while fast = slide */
        if ((m->input & INPUT_Z_PRESSED) && vec3f_magnitude_xz(m->vel) > TF_SLIDE_MIN_SPEED) {
            tf_instant_slide(m);
            m->particleFlags |= PARTICLE_MIST_CIRCLE;
            tf_sync_vel_to_mario(m);
            perform_ground_step(m);
            goto post_movement;
        }

        /* Jump */
        if ((m->input & INPUT_A_PRESSED) || gTFState.jumpBufferTimer > 0) {
            if (gTFState.jumpGraceTimer > 0) {
                tf_do_jump(m, TF_CVAR_F("Jump.Vel", TF_JUMP_VEL), 1.05f);
                m->action = ACT_FREEFALL;
                tf_sync_vel_to_mario(m);
                perform_air_step(m, 0);
                goto post_movement;
            }
        }

        /* Normal ground movement */
        tf_ground_move(m, dt);
        tf_sync_vel_to_mario(m);
        perform_ground_step(m);
    }

post_movement:
    /* ── Safety: re-check floor after movement stepped ── */
    if (m->floor == NULL) {
        /* OOB — try to recover by finding floor at current position */
        m->floorHeight = find_floor(m->pos[0], m->pos[1], m->pos[2], &m->floor);
        if (m->floor == NULL) {
            /* Truly OOB — bail, let vanilla handle recovery */
            return;
        }
    }

    /* ── Track state ──────────────────────────────────── */
    sWasAirborne = !onGround && !gTFState.slide.active;

    /* ── Face camera direction ────────────────────────── */
    {
        s16 camYawS16 = (s16)(gTFState.camera.yaw / 360.0f * 65536.0f);
        m->faceAngle[1] = camYawS16;
    }

    /* ── Sync graphics ────────────────────────────────── */
    vec3f_copy(m->marioObj->header.gfx.pos, m->pos);

    /*
     * ── Body tilt & Animation ────────────────────────────
     * Parkour feel: dynamic body angles + context-aware animations.
     * Body tilts into movement direction at high speed for momentum feel.
     */
    {
        f32 hspeed = vec3f_magnitude_xz(m->vel);
        s16 bodyRoll = 0;
        s16 bodyPitch = 0;

        if (gTFState.wallrun.active) {
            /* 45° lean into wall — feet on wall, body angled */
            s16 wallTilt = (s16)(45.0f / 360.0f * 65536.0f);
            bodyRoll = (gTFState.wallrun.side == 0) ? wallTilt : -wallTilt;
            set_mario_animation(m, MARIO_ANIM_RUNNING);
        } else if (sDiving) {
            bodyPitch = (s16)(-30.0f / 360.0f * 65536.0f);
            set_mario_animation(m, MARIO_ANIM_DIVE);
        } else if (sGroundPounding) {
            if (sGroundPoundTimer < 5) {
                set_mario_animation(m, MARIO_ANIM_START_GROUND_POUND);
            } else {
                bodyPitch = (s16)(20.0f / 360.0f * 65536.0f);
                set_mario_animation(m, MARIO_ANIM_GROUND_POUND);
            }
        } else if (gTFState.slide.active) {
            /* Low slide — pitch forward slightly for speed feel */
            bodyPitch = (s16)(-8.0f / 360.0f * 65536.0f);
            set_mario_animation(m, MARIO_ANIM_SLIDE_KICK);
        } else if (gTFState.wallKickTimer > 8) {
            set_mario_animation(m, MARIO_ANIM_START_WALLKICK);
        } else if (gTFState.wallKickTimer > 0) {
            set_mario_animation(m, MARIO_ANIM_FORWARD_SPINNING_FLIP);
        } else if (!onGround && m->vel[1] > 25.0f) {
            /* Fast rise — flip */
            set_mario_animation(m, MARIO_ANIM_FORWARD_SPINNING_FLIP);
        } else if (!onGround && m->vel[1] > 5.0f) {
            /* Rising */
            set_mario_animation(m, MARIO_ANIM_SINGLE_JUMP);
        } else if (!onGround && m->vel[1] > -15.0f) {
            /* Apex float */
            set_mario_animation(m, MARIO_ANIM_DOUBLE_JUMP_FALL);
        } else if (!onGround && hspeed > 30.0f) {
            /* Fast falling with momentum — airborne on stomach (superman) */
            set_mario_animation(m, MARIO_ANIM_AIRBORNE_ON_STOMACH);
            bodyPitch = (s16)(-15.0f / 360.0f * 65536.0f);
        } else if (!onGround) {
            set_mario_animation(m, MARIO_ANIM_GENERAL_FALL);
        } else if (hspeed > 32.0f) {
            /* Sprint — lean forward */
            bodyPitch = (s16)(-6.0f / 360.0f * 65536.0f);
            set_mario_animation(m, MARIO_ANIM_RUNNING);
        } else if (hspeed > 10.0f) {
            set_mario_animation(m, MARIO_ANIM_RUNNING);
        } else if (hspeed > 2.0f) {
            set_mario_animation(m, MARIO_ANIM_TIPTOE);
        } else {
            set_mario_animation(m, MARIO_ANIM_IDLE_HEAD_CENTER);
        }

        vec3s_set(m->marioObj->header.gfx.angle, bodyPitch, m->faceAngle[1], bodyRoll);
    }

    /* ── Speed particles ──────────────────────────────── */
    {
        f32 hspeed = vec3f_magnitude_xz(m->vel);
        if (hspeed > 40.0f && onGround) {
            m->particleFlags |= PARTICLE_DUST;
        }
        if (hspeed > 60.0f) {
            m->particleFlags |= PARTICLE_WAVE_TRAIL;
        }
    }

    /* ── Camera ───────────────────────────────────────── */
    tf_camera_update(m, dt);

    /* ── Weapon ───────────────────────────────────────── */
    tf_weapon_update(m);
}
