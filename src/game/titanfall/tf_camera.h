#ifndef TF_CAMERA_H
#define TF_CAMERA_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void tf_camera_update(struct MarioState *m, f32 dt);
void tf_camera_set_capture(u8 capture);
void tf_camera_init(void);

#ifdef __cplusplus
}
#endif

#endif /* TF_CAMERA_H */
