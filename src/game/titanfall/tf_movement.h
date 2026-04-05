#ifndef TF_MOVEMENT_H
#define TF_MOVEMENT_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Titanfall64 — Quake/Source/Titanfall movement for SM64
 *
 * All physics are dt-based (not frame-locked).
 * SM64 runs at 30hz game ticks; Ghostship interpolates to display refresh.
 */

/* ── Timing ─────────────────────────────────────────────────────── */
#define TF_TICKRATE           30.0f
#define TF_DT                 (1.0f / TF_TICKRATE)

/* ── Ground Movement ────────────────────────────────────────────── */
#define TF_GROUND_MAXSPEED    38.0f   /* sprint speed (tad slower, was 48)    */
#define TF_GROUND_ACCEL       10.0f
#define TF_GROUND_FRICTION     6.0f

/* ── Air Movement ───────────────────────────────────────────────── */
#define TF_AIR_ACCEL          12.0f   /* high = Titanfall feel                */
#define TF_AIR_CAP             4.0f   /* low = forces strafing, not hold-fwd  */
#define TF_GRAVITY           105.0f   /* units/sec^2 → 3.5/frame at 30fps    */
#define TF_TERMINAL_VEL      -75.0f   /* max downward velocity (per frame)    */

/* ── Jumping ────────────────────────────────────────────────────── */
#define TF_JUMP_VEL           60.0f   /* SM64's is ~52, slightly higher       */
#define TF_DOUBLE_JUMP_VEL    50.0f
#define TF_JUMP_GRACE_FRAMES   4      /* coyote time                          */
#define TF_JUMP_BUFFER_FRAMES  4      /* input buffer                         */

/* ── Slide ──────────────────────────────────────────────────────── */
#define TF_SLIDE_MIN_SPEED    20.0f   /* minimum speed to start slide         */
#define TF_SLIDE_BOOST         8.0f   /* speed added on entry                 */
#define TF_SLIDE_FRICTION      0.3f   /* very low (ground = 6.0)              */
#define TF_SLIDE_SLOPE_ACCEL  600.0f  /* downhill acceleration (halved from 1200) */
#define TF_SLIDE_SLOPE_DECEL  300.0f  /* uphill deceleration                   */
#define TF_SLIDE_MAX_SPEED   150.0f   /* high cap — slopes should go fast     */
#define TF_SLIDE_DURATION_MAX  90     /* max frames (~3 sec at 30hz)          */
#define TF_SLIDE_JUMP_BOOST    1.15f  /* speed multiplier on slide-jump       */
#define TF_SLIDE_HEIGHT_SCALE  0.5f   /* collision height reduction (crouch)  */
#define TF_SLIDE_VEL_CONVERT   0.6f   /* vertical-to-horizontal on entry      */
#define TF_SLIDE_STEER_ACCEL   1.2f   /* lateral steering while sliding       */

/* ── Wallrun ────────────────────────────────────────────────────── */
#define TF_WR_MIN_SPEED       20.0f   /* min horizontal speed to attach       */
#define TF_WR_MIN_HEIGHT      40.0f   /* min height above floor to attach     */
#define TF_WR_MAX_DURATION     45     /* max frames (~1.5 sec)                */
#define TF_WR_GRAVITY_SCALE    0.15f  /* gravity multiplier while wallrunning */
#define TF_WR_JUMP_OFF_SPEED  52.0f   /* wall-kick horizontal velocity        */
#define TF_WR_JUMP_UP_SPEED   42.0f   /* wall-kick vertical velocity          */
#define TF_WR_ATTACH_DOT       0.85f  /* max dot for attachment (too head-on) */
#define TF_WR_DETACH_DOT       0.3f   /* min dot for attachment (too parallel)*/
#define TF_WR_COOLDOWN         10     /* frames before reattach same wall     */
#define TF_WR_PUSH_FORCE       2.0f   /* force pushing into wall surface      */
#define TF_WR_SPEED_DECAY_RATE 0.008f /* per-frame speed decay multiplier     */
#define TF_WR_ENTRY_UPKICK    12.0f   /* upward bump on attach                */

/* ── Camera ─────────────────────────────────────────────────────── */
#define TF_CAM_SENSITIVITY     1.8f   /* mouselook sensitivity (lowered)      */
#define TF_CAM_PITCH_MAX      85.0f   /* degrees, prevent gimbal flip         */
#define TF_CAM_DISTANCE      700.0f   /* TPS distance behind player (0=FPS)   */
#define TF_CAM_HEIGHT         120.0f   /* height offset from player origin     */
#define TF_CAM_FOV             75.0f   /* field of view in degrees (SM64=45)   */
#define TF_CAM_WALLRUN_ROLL    5.0f   /* camera roll during wallrun (degrees) */

/* ── Weapon (L-STAR style) ──────────────────────────────────────── */
#define TF_WEAPON_COOLDOWN     4      /* frames between shots (7.5 rps)       */
#define TF_WEAPON_SPEED       60.0f   /* projectile speed (units/frame)       */
#define TF_WEAPON_LIFETIME    60      /* frames before despawn (~2s)          */
#define TF_WEAPON_SCALE        4.0f   /* projectile visual size               */
#define TF_WEAPON_SPAWN_FWD   50.0f   /* spawn distance in front of Mario     */
#define TF_CAM_ROLL_LERP       0.15f  /* roll interpolation speed             */

/* ── State structs ──────────────────────────────────────────────── */

struct WallrunState {
    u8 active;
    u8 side;              /* 0 = left wall, 1 = right wall        */
    s16 timer;
    s16 cooldown;
    Vec3f wallNormal;
    Vec3f runDir;
    f32 entrySpeed;
    struct Surface *lastWall;
};

struct SlideState {
    u8 active;
    s16 timer;
    f32 entrySpeed;
};

struct TFCamera {
    f32 yaw;              /* horizontal angle (degrees)            */
    f32 pitch;            /* vertical angle (degrees)              */
    f32 roll;             /* tilt (used for wallrun lean)          */
    f32 targetRoll;
    f32 distance;         /* 0 = FPS, >0 = TPS                    */
    Vec3f pos;
    Vec3f focus;
    u8 captured;          /* is mouse captured                     */
};

struct TFPlayerState {
    struct WallrunState wallrun;
    struct SlideState slide;
    struct TFCamera camera;
    s8 jumpGraceTimer;    /* coyote time countdown                 */
    s8 jumpBufferTimer;   /* input buffer countdown                */
    u8 canDoubleJump;
    u8 jumpsAvailable;    /* 2 = fresh, 1 = used one, 0 = spent   */
    u8 wallKickTimer;     /* frames since wall-kick (for animation)*/
    u8 shootCooldown;     /* frames until next shot allowed        */
    u8 mouseDown;         /* is left mouse button held             */
    u8 mousePressed;      /* left mouse just pressed this frame    */
    f32 lastGroundSpeed;  /* speed when last grounded              */
};

extern struct TFPlayerState gTFState;

/* ── API ────────────────────────────────────────────────────────── */

void tf_movement_init(void);
void tf_movement_update(struct MarioState *m);
void tf_camera_update(struct MarioState *m, f32 dt);
void tf_camera_set_capture(u8 capture);

/* Input */
void get_wishdir(struct MarioState *m, Vec3f wishdir, f32 *wishspeed);

/* Movement subsystems */
void tf_air_move(struct MarioState *m, f32 dt);
void tf_ground_move(struct MarioState *m, f32 dt);

/* Slide */
s32  tf_try_slide_enter(struct MarioState *m, struct SlideState *sl);
void tf_update_slide(struct MarioState *m, struct SlideState *sl, f32 dt);
void tf_slide_jump(struct MarioState *m, struct SlideState *sl);
void tf_slide_exit(struct MarioState *m, struct SlideState *sl);

/* Wallrun */
/* Weapon */
void tf_weapon_update(struct MarioState *m);
void tf_projectile_update(void);

s32  tf_try_wallrun_attach(struct MarioState *m, struct WallrunState *wr);
void tf_update_wallrun(struct MarioState *m, struct WallrunState *wr, f32 dt);
void tf_wallrun_jump(struct MarioState *m, struct WallrunState *wr);
void tf_wallrun_detach(struct MarioState *m, struct WallrunState *wr);

#ifdef __cplusplus
}
#endif

#endif /* TF_MOVEMENT_H */
