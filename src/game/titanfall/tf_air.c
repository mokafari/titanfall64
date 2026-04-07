/*
 * tf_air.c — Quake III-style air strafing
 *
 * The core mechanic: air acceleration with capped wishspeed.
 * When strafing, wishdir is ~perpendicular to velocity →
 * dot(vel, wishdir) ~ 0 → full accel always applies → speed grows.
 *
 * TF_AIR_CAP (4.0) forces player to strafe. Hold-forward gives no gain.
 * This is what makes bunny hopping and air strafing work.
 */

#include <math.h>
#include "sm64.h"
#include "game/mario.h"
#include "game/titanfall/tf_movement.h"
#include "game/titanfall/tf_math.h"

static void tf_air_accelerate(struct MarioState *m, Vec3f wishdir, f32 wishspeed, f32 dt) {
    f32 cappedSpeed = wishspeed;
    if (cappedSpeed > TF_CVAR_F("Air.Cap", TF_AIR_CAP)) {
        cappedSpeed = TF_CVAR_F("Air.Cap", TF_AIR_CAP);
    }

    f32 currentSpeed = m->vel[0] * wishdir[0] + m->vel[2] * wishdir[2];
    f32 addSpeed = cappedSpeed - currentSpeed;
    if (addSpeed <= 0.0f) {
        return;
    }

    f32 accelSpeed = TF_CVAR_F("Air.Accel", TF_AIR_ACCEL) * wishspeed * dt;
    if (accelSpeed > addSpeed) {
        accelSpeed = addSpeed;
    }

    m->vel[0] += accelSpeed * wishdir[0];
    m->vel[2] += accelSpeed * wishdir[2];
}

/*
 * tf_air_move — horizontal air acceleration only.
 * Gravity handled by perform_air_step's apply_gravity (4.0/frame).
 * Double jump handled in tf_movement.c main loop.
 */
void tf_air_move(struct MarioState *m, f32 dt) {
    Vec3f wishdir;
    f32 wishspeed;
    get_wishdir(m, wishdir, &wishspeed);

    if (wishspeed > 0.01f) {
        tf_air_accelerate(m, wishdir, wishspeed, dt);
    }
}
