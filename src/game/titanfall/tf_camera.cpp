/*
 * tf_camera.cpp — FPS/TPS mouselook camera
 *
 * C++ file for libultraship access (mouse delta, capture).
 * All functions use extern "C" linkage for C game code.
 *
 * Uses SM64's own sins/coss for coordinate system consistency:
 *   sins(angle) → X component, coss(angle) → Z component
 *   angle 0 = facing +Z direction
 */

#include <libultraship.h>
#include <cmath>

/* sm64.h includes Engine.h (C++) — must NOT be inside extern "C" */
#include "sm64.h"

extern "C" {
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "game/camera.h"
#include "game/game_init.h"
#include "game/titanfall/tf_movement.h"
#include "game/titanfall/tf_camera.h"
#include "game/titanfall/tf_math.h"

/* FOV override — sFOVState declared in camera.c */
extern struct CameraFOVStatus sFOVState;

}

/* MSVC mangles struct-typed extern variables even inside extern "C" blocks.
   Use a pointer obtained from a C helper to avoid the linkage mismatch. */
extern "C" void *tf_get_lakitu_state_ptr(void);
#define gLakituState (*(struct LakituState *)tf_get_lakitu_state_ptr())

/* ── Mouse I/O via libultraship ─────────────────────────────────── */

static void tf_read_mouse_delta(f32 *dx, f32 *dy) {
    auto window = Ship::Context::GetInstance()->GetWindow();
    if (window == nullptr) {
        *dx = 0.0f;
        *dy = 0.0f;
        return;
    }
    auto coords = window->GetMouseDelta();
    *dx = (f32)coords.x;
    *dy = (f32)coords.y;
}

static bool tf_read_mouse_button(int btn) {
    auto window = Ship::Context::GetInstance()->GetWindow();
    if (window == nullptr) return false;
    return window->GetMouseState((Ship::MouseBtn)btn);
}

static void tf_mouse_capture(bool capture) {
    auto window = Ship::Context::GetInstance()->GetWindow();
    if (window != nullptr) {
        window->SetMouseCapture(capture);
    }
}

/* ── Camera Init ────────────────────────────────────────────────── */

extern "C" void tf_camera_init(void) {
    gTFState.camera.yaw = 0.0f;
    gTFState.camera.pitch = 15.0f;  /* slight downward look to start */
    gTFState.camera.roll = 0.0f;
    gTFState.camera.targetRoll = 0.0f;
    gTFState.camera.distance = TF_CAM_DISTANCE;
    gTFState.camera.captured = 0;
}

/* ── Per-frame Update ───────────────────────────────────────────── */

extern "C" void tf_camera_update(struct MarioState *m, f32 dt) {
    struct TFCamera *cam = &gTFState.camera;
    (void)dt;

    /* Safety: bail if Mario state isn't ready */
    if (m == NULL || m->marioObj == NULL) {
        return;
    }

    /* On first frame, initialize yaw from Mario's facing direction */
    static u8 sFirstFrame = 1;
    if (sFirstFrame) {
        /* Convert Mario's s16 faceAngle to degrees */
        cam->yaw = (f32)m->faceAngle[1] * (360.0f / 65536.0f);
        sFirstFrame = 0;
    }

    /* ── Mouse input ──────────────────────────────────── */
    f32 mouseDX = 0.0f, mouseDY = 0.0f;
    if (cam->captured) {
        tf_read_mouse_delta(&mouseDX, &mouseDY);
    }

    f32 sensitivity = TF_CVAR_F("Cam.Sensitivity", TF_CAM_SENSITIVITY);
    cam->yaw   -= mouseDX * sensitivity;
    cam->pitch -= mouseDY * sensitivity;

    /* Clamp pitch */
    if (cam->pitch > TF_CAM_PITCH_MAX) cam->pitch = TF_CAM_PITCH_MAX;
    if (cam->pitch < -TF_CAM_PITCH_MAX) cam->pitch = -TF_CAM_PITCH_MAX;

    /* Wrap yaw */
    while (cam->yaw > 360.0f)  cam->yaw -= 360.0f;
    while (cam->yaw < 0.0f)    cam->yaw += 360.0f;

    /* Wallrun roll interpolation */
    cam->roll += (cam->targetRoll - cam->roll) * TF_CAM_ROLL_LERP;
    if (fabsf(cam->roll) < 0.1f && cam->targetRoll == 0.0f) {
        cam->roll = 0.0f;
    }

    /* ── Convert angles to SM64 s16 for trig ──────────── */
    s16 yawS16   = (s16)(cam->yaw   / 360.0f * 65536.0f);
    s16 pitchS16 = (s16)(cam->pitch / 360.0f * 65536.0f);

    /* ── Focus point: Mario's head ────────────────────── */
    cam->focus[0] = m->pos[0];
    cam->focus[1] = m->pos[1] + TF_CVAR_F("Cam.Height", TF_CAM_HEIGHT);
    cam->focus[2] = m->pos[2];

    cam->distance = TF_CVAR_F("Cam.Distance", TF_CAM_DISTANCE);
    if (cam->distance < 1.0f) {
        /* FPS mode — camera at Mario's head */
        cam->pos[0] = cam->focus[0];
        cam->pos[1] = cam->focus[1];
        cam->pos[2] = cam->focus[2];
    } else {
        /*
         * TPS mode — camera behind Mario.
         *
         * SM64 convention: forward = (sins(yaw), 0, coss(yaw))
         * "Behind" is the opposite: (-sins(yaw), 0, -coss(yaw))
         * Pitch tilts the camera up/down relative to focus.
         *
         * Camera position = focus - forward * distance * cos(pitch)
         *                         + up * distance * sin(pitch)
         */
        f32 horizDist = cam->distance * coss(pitchS16);
        f32 vertDist  = cam->distance * sins(pitchS16);

        cam->pos[0] = cam->focus[0] - sins(yawS16) * horizDist;
        cam->pos[1] = cam->focus[1] + vertDist;
        cam->pos[2] = cam->focus[2] - coss(yawS16) * horizDist;

        /* Basic floor/ceiling clipping for camera */
        struct Surface *surf = NULL;
        f32 floorY = find_floor(cam->pos[0], cam->pos[1] + 200.0f, cam->pos[2], &surf);
        if (surf != NULL && cam->pos[1] < floorY + 80.0f) {
            cam->pos[1] = floorY + 80.0f;
        }

        f32 ceilY = find_ceil(cam->pos[0], cam->pos[1] - 50.0f, cam->pos[2], &surf);
        if (surf != NULL && cam->pos[1] > ceilY - 50.0f) {
            cam->pos[1] = ceilY - 50.0f;
        }
    }

    /* ── Write to SM64 camera system ──────────────────── */
    gLakituState.pos[0]      = cam->pos[0];
    gLakituState.pos[1]      = cam->pos[1];
    gLakituState.pos[2]      = cam->pos[2];
    gLakituState.focus[0]    = cam->focus[0];
    gLakituState.focus[1]    = cam->focus[1];
    gLakituState.focus[2]    = cam->focus[2];
    gLakituState.curPos[0]   = cam->pos[0];
    gLakituState.curPos[1]   = cam->pos[1];
    gLakituState.curPos[2]   = cam->pos[2];
    gLakituState.curFocus[0] = cam->focus[0];
    gLakituState.curFocus[1] = cam->focus[1];
    gLakituState.curFocus[2] = cam->focus[2];
    gLakituState.goalPos[0]  = cam->pos[0];
    gLakituState.goalPos[1]  = cam->pos[1];
    gLakituState.goalPos[2]  = cam->pos[2];
    gLakituState.goalFocus[0]= cam->focus[0];
    gLakituState.goalFocus[1]= cam->focus[1];
    gLakituState.goalFocus[2]= cam->focus[2];

    /* Roll for wallrun tilt */
    gLakituState.roll = (s16)(cam->roll / 360.0f * 65536.0f);

    /* Yaw for vanilla systems that read it */
    gLakituState.yaw = yawS16;
    gLakituState.nextYaw = yawS16;

    /* Update area camera yaw so vanilla intendedYaw still works as fallback */
    if (m->area != NULL && m->area->camera != NULL) {
        m->area->camera->yaw = yawS16;
    }

    /* Override FOV */
    sFOVState.fov = TF_CVAR_F("Cam.FOV", TF_CAM_FOV);

    /* ── Mouse button state (for weapon) ──────────────── */
    {
        u8 wasDown = gTFState.mouseDown;
        gTFState.mouseDown = tf_read_mouse_button(0) ? 1 : 0;  /* 0 = LUS_MOUSE_BTN_LEFT */
        gTFState.mousePressed = (!wasDown && gTFState.mouseDown) ? 1 : 0;
    }
}

/* ── Mouse capture toggle ───────────────────────────────────────── */

extern "C" void tf_camera_set_capture(u8 capture) {
    gTFState.camera.captured = capture;
    tf_mouse_capture(capture != 0);
}
