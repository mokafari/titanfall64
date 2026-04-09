/*
 * tf_ground.c — Quake-style ground movement
 *
 * Order: friction FIRST, then acceleration.
 * Critical for bhop: friction only applies on ground frames.
 * If you bhop (jump on landing frame), you skip ground friction entirely.
 */

#include <math.h>
#include "sm64.h"
#include "game/mario.h"
#include "game/titanfall/tf_movement.h"
#include "game/titanfall/tf_math.h"

void tf_ground_move(struct MarioState *m, f32 dt) {
    f32 speed = vec3f_magnitude_xz(m->vel);

    /*
     * Friction first — this is why bhop works.
     * If you jump on the landing frame, tf_movement_update sends you
     * straight to air without ever calling tf_ground_move.
     * So friction never applies → speed is preserved.
     */
    if (speed > 0.1f) {
        f32 drop = speed * TF_CVAR_F("Ground.Friction", TF_GROUND_FRICTION) * dt;
        f32 newspeed = speed - drop;
        if (newspeed < 0.0f) newspeed = 0.0f;
        f32 scale = newspeed / speed;
        m->vel[0] *= scale;
        m->vel[2] *= scale;
    }

    /* Acceleration */
    Vec3f wishdir;
    f32 wishspeed;
    get_wishdir(m, wishdir, &wishspeed);

    if (wishspeed < 0.01f) {
        /* No input: apply extra stopping friction so Mario doesn't slide */
        speed = vec3f_magnitude_xz(m->vel);
        if (speed > 0.1f) {
            f32 stopDrop = speed * 12.0f * dt;  /* 2x normal friction for stopping */
            f32 newspeed = speed - stopDrop;
            if (newspeed < 1.0f) newspeed = 0.0f;
            f32 scale = newspeed / speed;
            m->vel[0] *= scale;
            m->vel[2] *= scale;
        }
        return;
    }

    f32 currentSpeed = m->vel[0] * wishdir[0] + m->vel[2] * wishdir[2];
    f32 addSpeed = wishspeed - currentSpeed;
    if (addSpeed <= 0.0f) {
        return;
    }

    f32 accelSpeed = TF_CVAR_F("Ground.Accel", TF_GROUND_ACCEL) * wishspeed * dt;
    if (accelSpeed > addSpeed) {
        accelSpeed = addSpeed;
    }

    m->vel[0] += accelSpeed * wishdir[0];
    m->vel[2] += accelSpeed * wishdir[2];
}

/* tf_try_jump removed — jumping is now handled in tf_movement.c main loop */
