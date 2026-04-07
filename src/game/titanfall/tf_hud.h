#ifndef TF_HUD_H
#define TF_HUD_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── HUD State ─────────────────────────────────────────────────── */

struct TFHudState {
    s32 wallrunChain;       /* current wallrun chain count          */
    s32 bestChain;          /* best chain this session              */
    f32 topSpeed;           /* highest speed reached                */
    f32 displaySpeed;       /* smoothed speed for display           */
    s16 speedFlashTimer;    /* frames remaining for flash effect    */
    s16 perfectBhopFlash;   /* frames remaining for bhop flash     */
};

extern struct TFHudState gTFHud;

/* ── Speed thresholds ──────────────────────────────────────────── */

#define TF_HUD_SPEED_WALK      48.0f
#define TF_HUD_SPEED_SPRINT    65.0f
#define TF_HUD_SPEED_FAST      80.0f
#define TF_HUD_SPEED_MAX      100.0f   /* bar is full at this speed */
#define TF_HUD_CHAIN_THRESHOLD 80.0f   /* speed threshold for chain */

/* ── Bar layout (320x240 virtual resolution) ───────────────────── */

#define TF_HUD_BAR_X           80
#define TF_HUD_BAR_Y          220
#define TF_HUD_BAR_W          160
#define TF_HUD_BAR_H            8

/* ── Flash durations ───────────────────────────────────────────── */

#define TF_HUD_FLASH_DURATION  15      /* frames for speed flash    */
#define TF_HUD_BHOP_FLASH_DUR  10      /* frames for bhop flash     */

/* ── API ───────────────────────────────────────────────────────── */

void tf_hud_init(void);
void tf_hud_update(struct MarioState *m);
void tf_hud_render(void);

/* Call from wallrun code to increment chain */
void tf_hud_wallrun_chain_increment(void);
void tf_hud_wallrun_chain_reset(void);
void tf_hud_notify_perfect_bhop(void);

#ifdef __cplusplus
}
#endif

#endif /* TF_HUD_H */
