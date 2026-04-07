/*
 * tf_grapple.h -- Titanfall64 grapple/hook system
 *
 * R button fires a grapple ray from camera direction.
 * If it hits geometry within range, pulls Mario toward the hit point
 * with swing physics (preserved lateral momentum, reduced gravity).
 */

#ifndef TF_GRAPPLE_H
#define TF_GRAPPLE_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Extra grapple-specific constants (base constants + struct in tf_movement.h) */
#define TF_GRAPPLE_GRAVITY_SCALE     0.3f
#define TF_GRAPPLE_STRAFE_SCALE      0.3f
#define TF_GRAPPLE_RAY_STEP         40.0f

#include "game/titanfall/tf_movement.h"

#ifdef __cplusplus
}
#endif

#endif /* TF_GRAPPLE_H */
