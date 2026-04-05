#ifndef TF_MATH_H
#define TF_MATH_H

#include "types.h"
#include <math.h>

static inline f32 vec3f_dot_xz(Vec3f a, Vec3f b) {
    return a[0] * b[0] + a[2] * b[2];
}

static inline f32 vec3f_magnitude_xz(Vec3f v) {
    return sqrtf(v[0] * v[0] + v[2] * v[2]);
}

static inline void vec3f_normalize_xz(Vec3f v) {
    f32 len = sqrtf(v[0] * v[0] + v[2] * v[2]);
    if (len > 0.001f) {
        v[0] /= len;
        v[2] /= len;
    }
}

static inline f32 tf_fmaxf(f32 a, f32 b) { return a > b ? a : b; }
static inline f32 tf_fminf(f32 a, f32 b) { return a < b ? a : b; }
static inline f32 tf_clampf(f32 v, f32 lo, f32 hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline f32 tf_sqrtf(f32 x) {
    return (x > 0.0f) ? sqrtf(x) : 0.0f;
}

#endif /* TF_MATH_H */
