/*
 * tf_slide.c — Titanfall-style crouch slide
 *
 * Trigger: Z while moving fast on ground.
 * Low friction, slope interaction, limited steering.
 * Slide -> slide-jump -> airstrafe -> land -> slide = the bhop loop.
 */

#include <math.h>
#include "sm64.h"
#include "game/mario.h"
#include "game/titanfall/tf_movement.h"
#include "game/titanfall/tf_math.h"

s32 tf_try_slide_enter(struct MarioState *m, struct SlideState *sl) {
    if (!(m->input & INPUT_Z_PRESSED)) return 0;

    f32 hspeed = vec3f_magnitude_xz(m->vel);
    if (hspeed < TF_SLIDE_MIN_SPEED) return 0;

    sl->active = 1;
    sl->timer = 0;
    sl->entrySpeed = hspeed;

    /* Convert downward velocity to horizontal speed */
    if (m->vel[1] < -10.0f) {
        f32 convert = -m->vel[1] * TF_SLIDE_VEL_CONVERT;
        f32 velLen = vec3f_magnitude_xz(m->vel);
        if (velLen > 0.01f) {
            m->vel[0] += (m->vel[0] / velLen) * convert;
            m->vel[2] += (m->vel[2] / velLen) * convert;
        }
        m->vel[1] = 0.0f;
    }

    /* Entry boost */
    f32 velLen = vec3f_magnitude_xz(m->vel);
    if (velLen > 0.01f) {
        m->vel[0] += (m->vel[0] / velLen) * TF_SLIDE_BOOST;
        m->vel[2] += (m->vel[2] / velLen) * TF_SLIDE_BOOST;
    }

    return 1;
}

void tf_update_slide(struct MarioState *m, struct SlideState *sl, f32 dt) {
    sl->timer++;
    f32 hspeed = vec3f_magnitude_xz(m->vel);

    /* Exit conditions — be generous, don't end too early */
    if (sl->timer > TF_SLIDE_DURATION_MAX || m->floor == NULL) {
        tf_slide_exit(m, sl);
        return;
    }

    /* Only check speed/input after a minimum duration (15 frames = 0.5s) */
    if (sl->timer > 15) {
        if (hspeed < 5.0f) {
            tf_slide_exit(m, sl);
            return;
        }
        if (!(m->input & INPUT_Z_DOWN)) {
            tf_slide_exit(m, sl);
            return;
        }
    }

    /* Slide-jump (the money move) */
    if (m->input & INPUT_A_PRESSED) {
        tf_slide_jump(m, sl);
        return;
    }

    /* Low friction */
    if (hspeed > 0.1f) {
        f32 drop = hspeed * TF_SLIDE_FRICTION * dt;
        f32 newspeed = hspeed - drop;
        if (newspeed < 0.0f) newspeed = 0.0f;
        f32 scale = newspeed / hspeed;
        m->vel[0] *= scale;
        m->vel[2] *= scale;
    }

    /* Slope interaction — surface normals are f32 in Ghostship */
    if (m->floor != NULL) {
        f32 nx = m->floor->normal.x;
        f32 nz = m->floor->normal.z;

        /* Floor normal points UP from surface. The XZ components point
         * UPHILL (away from the slope's low side). So the DOWNHILL
         * direction is the same sign as the normal's XZ — gravity
         * pulls you in the direction the normal tilts. */
        f32 slopeDirX = nx;
        f32 slopeDirZ = nz;
        f32 slopeMag = sqrtf(slopeDirX * slopeDirX + slopeDirZ * slopeDirZ);

        if (slopeMag > 0.01f) {
            /*
             * CS:Source surf-style slope physics:
             * Gravity always pulls downhill on a slope.
             * The steeper the slope (larger slopeMag), the stronger the pull.
             * This makes downhill slides accelerate and uphill slides brake.
             */
            f32 accel = TF_SLIDE_SLOPE_ACCEL * slopeMag * slopeMag * dt;
            m->vel[0] += slopeDirX * accel;
            m->vel[2] += slopeDirZ * accel;
        }
    }

    /*
     * Steering — rotate velocity toward wish direction without adding speed.
     * Mix a small amount of wishdir into the velocity, then rescale to
     * preserve the original speed. This redirects without accelerating.
     */
    Vec3f wishdir;
    f32 wishspeed;
    get_wishdir(m, wishdir, &wishspeed);
    hspeed = vec3f_magnitude_xz(m->vel);
    if (wishspeed > 0.1f && hspeed > 1.0f) {
        f32 steerStrength = 0.06f;  /* how fast the slide turns */
        m->vel[0] = m->vel[0] * (1.0f - steerStrength) + wishdir[0] * hspeed * steerStrength;
        m->vel[2] = m->vel[2] * (1.0f - steerStrength) + wishdir[2] * hspeed * steerStrength;

        /* Rescale to preserve speed — steering redirects, doesn't accelerate */
        f32 newSpeed = vec3f_magnitude_xz(m->vel);
        if (newSpeed > 0.01f) {
            f32 ratio = hspeed / newSpeed;
            m->vel[0] *= ratio;
            m->vel[2] *= ratio;
        }
    }

    /* Speed cap */
    hspeed = vec3f_magnitude_xz(m->vel);
    if (hspeed > TF_SLIDE_MAX_SPEED) {
        f32 ratio = TF_SLIDE_MAX_SPEED / hspeed;
        m->vel[0] *= ratio;
        m->vel[2] *= ratio;
    }

    /* Stay snapped to ground */
    m->vel[1] = -2.0f;
}

void tf_slide_jump(struct MarioState *m, struct SlideState *sl) {
    m->vel[0] *= TF_SLIDE_JUMP_BOOST;
    m->vel[2] *= TF_SLIDE_JUMP_BOOST;
    m->vel[1] = TF_JUMP_VEL * 0.85f;
    sl->active = 0;
    sl->timer = 0;
    gTFState.canDoubleJump = 1;
}

void tf_slide_exit(struct MarioState *m, struct SlideState *sl) {
    (void)m;
    sl->active = 0;
    sl->timer = 0;
}
