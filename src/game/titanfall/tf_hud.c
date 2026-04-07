#include "sm64.h"
#include "engine/math_util.h"
#include "game/game_init.h"
#include "game/print.h"
#include "game/titanfall/tf_hud.h"
#include "game/titanfall/tf_movement.h"
#include "game/titanfall/tf_math.h"

/* ── Global HUD state ──────────────────────────────────────────── */

struct TFHudState gTFHud;

/* ── Internal helpers ──────────────────────────────────────────── */

/**
 * Pack an RGBA color into the doubled 16-bit format that gDPSetFillColor expects.
 * In fill mode the RDP wants (rgba5551 << 16 | rgba5551).
 */
static u32 tf_hud_pack_fill_color(u8 r, u8 g, u8 b, u8 a) {
    u16 c = GPACK_RGBA5551(r, g, b, a);
    return (u32)(c << 16 | c);
}

/**
 * Draw a filled rectangle on the HUD.
 * Switches to fill cycle type, draws, then restores 1-cycle mode.
 */
static void tf_hud_fill_rect(s32 x, s32 y, s32 w, s32 h, u8 r, u8 g, u8 b) {
    if (w <= 0 || h <= 0) {
        return;
    }

    gDPPipeSync(gDisplayListHead++);
    gDPSetRenderMode(gDisplayListHead++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCycleType(gDisplayListHead++, G_CYC_FILL);
    gDPSetFillColor(gDisplayListHead++, tf_hud_pack_fill_color(r, g, b, 1));
    gDPFillRectangle(gDisplayListHead++, x, y, x + w - 1, y + h - 1);
    gDPPipeSync(gDisplayListHead++);
    gDPSetCycleType(gDisplayListHead++, G_CYC_1CYCLE);
}

/**
 * Get the speed bar color based on current speed.
 * Outputs r, g, b values.
 */
static void tf_hud_speed_color(f32 speed, u8 *r, u8 *g, u8 *b) {
    if (speed >= TF_HUD_SPEED_FAST) {
        /* Red: chain speed */
        *r = 255; *g = 50;  *b = 50;
    } else if (speed >= TF_HUD_SPEED_SPRINT) {
        /* Orange: fast */
        *r = 255; *g = 140; *b = 40;
    } else if (speed >= TF_HUD_SPEED_WALK) {
        /* Yellow: sprint */
        *r = 255; *g = 230; *b = 40;
    } else {
        /* White: walk */
        *r = 220; *g = 220; *b = 220;
    }
}

/* ── Public API ────────────────────────────────────────────────── */

void tf_hud_init(void) {
    gTFHud.wallrunChain    = 0;
    gTFHud.bestChain       = 0;
    gTFHud.topSpeed        = 0.0f;
    gTFHud.displaySpeed    = 0.0f;
    gTFHud.speedFlashTimer = 0;
    gTFHud.perfectBhopFlash = 0;
}

void tf_hud_wallrun_chain_increment(void) {
    gTFHud.wallrunChain++;
    if (gTFHud.wallrunChain > gTFHud.bestChain) {
        gTFHud.bestChain = gTFHud.wallrunChain;
    }
}

void tf_hud_wallrun_chain_reset(void) {
    gTFHud.wallrunChain = 0;
}

void tf_hud_notify_perfect_bhop(void) {
    gTFHud.perfectBhopFlash = TF_HUD_BHOP_FLASH_DUR;
}

void tf_hud_update(struct MarioState *m) {
    f32 rawSpeed = vec3f_magnitude_xz(m->vel);

    /* Smooth the display speed (lerp toward actual) */
    f32 lerpRate = 0.25f;
    gTFHud.displaySpeed += (rawSpeed - gTFHud.displaySpeed) * lerpRate;

    /* Track top speed */
    if (rawSpeed > gTFHud.topSpeed) {
        gTFHud.topSpeed = rawSpeed;
    }

    /* Trigger flash when crossing chain threshold */
    if (rawSpeed >= TF_HUD_CHAIN_THRESHOLD && gTFHud.displaySpeed < TF_HUD_CHAIN_THRESHOLD) {
        gTFHud.speedFlashTimer = TF_HUD_FLASH_DURATION;
    }

    /* Tick down flash timers */
    if (gTFHud.speedFlashTimer > 0) {
        gTFHud.speedFlashTimer--;
    }
    if (gTFHud.perfectBhopFlash > 0) {
        gTFHud.perfectBhopFlash--;
    }
}

void tf_hud_render(void) {
    f32 speed = gTFHud.displaySpeed;
    f32 fillRatio = speed / TF_HUD_SPEED_MAX;
    s32 fillW;
    u8 r, g, b;

    /* Clamp fill ratio to [0, 1] */
    if (fillRatio < 0.0f) fillRatio = 0.0f;
    if (fillRatio > 1.0f) fillRatio = 1.0f;

    fillW = (s32)(fillRatio * TF_HUD_BAR_W);

    /* ── Background bar (dark) ─────────────────────────────────── */
    tf_hud_fill_rect(TF_HUD_BAR_X, TF_HUD_BAR_Y, TF_HUD_BAR_W, TF_HUD_BAR_H,
                     20, 20, 20);

    /* ── Filled speed bar ──────────────────────────────────────── */
    if (fillW > 0) {
        /* Check for flash override: white flash when crossing chain threshold */
        if (gTFHud.speedFlashTimer > 0 && (gTFHud.speedFlashTimer & 2)) {
            r = 255; g = 255; b = 255;
        } else {
            tf_hud_speed_color(speed, &r, &g, &b);
        }

        /* Perfect bhop flash: bright cyan pulse */
        if (gTFHud.perfectBhopFlash > 0 && (gTFHud.perfectBhopFlash & 1)) {
            r = 100; g = 255; b = 255;
        }

        tf_hud_fill_rect(TF_HUD_BAR_X, TF_HUD_BAR_Y, fillW, TF_HUD_BAR_H, r, g, b);
    }

    /* ── Numeric speed display (right of bar) ──────────────────── */
    /* print_text_fmt_int uses top-left origin with y=0 at bottom,
     * SM64 print coords: x is left edge (0-320), y is from bottom (0-240).
     * Convert screen y (top-down) to print y (bottom-up): printY = 240 - screenY */
    {
        s32 printX = TF_HUD_BAR_X + TF_HUD_BAR_W + 4;
        s32 printY = 240 - TF_HUD_BAR_Y;
        s32 speedInt = (s32)speed;
        print_text_fmt_int(printX, printY, "%d", speedInt);
    }

    /* ── Chain counter (above bar, only when chain > 0) ────────── */
    if (gTFHud.wallrunChain > 0) {
        s32 chainX = TF_HUD_BAR_X + (TF_HUD_BAR_W / 2) - 16;
        s32 chainY = 240 - (TF_HUD_BAR_Y - 14);
        print_text_fmt_int(chainX, chainY, "CHAIN %d", gTFHud.wallrunChain);
    }
}
