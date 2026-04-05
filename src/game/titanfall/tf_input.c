/*
 * tf_input.c — Wishdir calculation from stick + camera
 *
 * Converts SM64's analog stick input into a Quake-style wishdir + wishspeed pair.
 * All three movement systems (air, ground, slide) consume this.
 *
 * Uses SM64's own sins/coss with s16 angles to match the coordinate system:
 *   - sins(angle) → X component
 *   - coss(angle) → Z component
 *   - angle 0 = facing +Z direction
 */

#include <math.h>
#include "sm64.h"
#include "engine/math_util.h"
#include "game/mario.h"
#include "game/titanfall/tf_movement.h"
#include "game/titanfall/tf_math.h"

void get_wishdir(struct MarioState *m, Vec3f wishdir, f32 *wishspeed) {
    wishdir[0] = 0.0f;
    wishdir[1] = 0.0f;
    wishdir[2] = 0.0f;
    *wishspeed = 0.0f;

    f32 stickMag = m->controller->stickMag;  /* 0..64 */
    if (stickMag < 8.0f) {
        return;
    }

    /* Normalize stick magnitude to 0..1 with deadzone */
    f32 normalizedMag = (stickMag - 8.0f) / 56.0f;
    if (normalizedMag > 1.0f) normalizedMag = 1.0f;

    f32 stickX = -m->controller->stickX; /* flip: SM64's X convention is opposite */
    f32 stickY = m->controller->stickY;  /* [-64, 64] up/forward positive */

    f32 inputLen = sqrtf(stickX * stickX + stickY * stickY);
    if (inputLen < 0.01f) {
        return;
    }

    f32 normX = stickX / inputLen;   /* right component, normalized */
    f32 normY = stickY / inputLen;   /* forward component, normalized */

    /*
     * Convert camera yaw from degrees to SM64 s16 angle.
     * SM64 coordinate convention:
     *   Forward vector from angle α: X = sins(α), Z = coss(α)
     *   Right vector from angle α:   X = coss(α), Z = -sins(α)
     *
     * stickY = forward (push up = move in camera's forward direction)
     * stickX = right (push right = move to camera's right)
     */
    s16 camYawS16 = (s16)(gTFState.camera.yaw / 360.0f * 65536.0f);

    f32 fwdX = sins(camYawS16);
    f32 fwdZ = coss(camYawS16);
    f32 rightX = coss(camYawS16);
    f32 rightZ = -sins(camYawS16);

    /* Compose: forward * stickY + right * stickX */
    wishdir[0] = normY * fwdX + normX * rightX;
    wishdir[1] = 0.0f;
    wishdir[2] = normY * fwdZ + normX * rightZ;

    /* Normalize the direction */
    f32 len = sqrtf(wishdir[0] * wishdir[0] + wishdir[2] * wishdir[2]);
    if (len > 0.001f) {
        wishdir[0] /= len;
        wishdir[2] /= len;
    }

    *wishspeed = normalizedMag * TF_GROUND_MAXSPEED;
}
