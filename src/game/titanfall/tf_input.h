#ifndef TF_INPUT_H
#define TF_INPUT_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void get_wishdir(struct MarioState *m, Vec3f wishdir, f32 *wishspeed);

#ifdef __cplusplus
}
#endif

#endif /* TF_INPUT_H */
