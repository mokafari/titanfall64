/*
 * tf_weapon.c — L-STAR style energy projectile weapon
 *
 * Left mouse button fires slow-moving glowing projectiles.
 * Projectiles are red flame billboards that fly in a line and
 * damage enemies via INTERACT_FLAME on contact.
 *
 * We manage projectile tracking ourselves (position, velocity, lifetime)
 * and update the spawned SM64 objects each frame. This avoids fighting
 * with existing flame behavior scripts.
 */

#include <math.h>
#include "sm64.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "game/mario.h"
#include "game/interaction.h"
#include "game/object_helpers.h"
#include "game/object_list_processor.h"
#include "game/titanfall/tf_movement.h"
#include "game/titanfall/tf_math.h"
#include "behavior_data.h"
#include "model_ids.h"
#include "object_fields.h"
#include "sounds.h"
#include "audio/external.h"

#define TF_MAX_PROJECTILES 16

struct TfProjectile {
    struct Object *obj;   /* SM64 object (flame visual)          */
    Vec3f pos;            /* current position                    */
    Vec3f vel;            /* velocity (units/frame)              */
    s16 timer;            /* frames alive                        */
    u8 active;
};

static struct TfProjectile sProjectiles[TF_MAX_PROJECTILES] = { 0 };

/* Find a free slot */
static struct TfProjectile *tf_alloc_projectile(void) {
    for (s32 i = 0; i < TF_MAX_PROJECTILES; i++) {
        if (!sProjectiles[i].active) {
            return &sProjectiles[i];
        }
    }
    /* Recycle oldest */
    return &sProjectiles[0];
}

/* ── Update all active projectiles ──────────────────────────────── */

static void tf_update_projectiles(void) {
    for (s32 i = 0; i < TF_MAX_PROJECTILES; i++) {
        struct TfProjectile *p = &sProjectiles[i];
        if (!p->active) continue;

        p->timer++;

        /* Move */
        p->pos[0] += p->vel[0];
        p->pos[1] += p->vel[1];
        p->pos[2] += p->vel[2];

        /* Slight gravity arc */
        p->vel[1] -= 0.4f;

        /* Lifetime check */
        if (p->timer > TF_WEAPON_LIFETIME) {
            if (p->obj != NULL) {
                obj_mark_for_deletion(p->obj);
            }
            p->active = 0;
            p->obj = NULL;
            continue;
        }

        /* Floor collision */
        struct Surface *floor = NULL;
        f32 floorY = find_floor(p->pos[0], p->pos[1] + 50.0f, p->pos[2], &floor);
        if (floor != NULL && p->pos[1] < floorY + 15.0f) {
            if (p->obj != NULL) {
                obj_mark_for_deletion(p->obj);
            }
            p->active = 0;
            p->obj = NULL;
            continue;
        }

        /* Update the SM64 object position */
        if (p->obj != NULL) {
            p->obj->oPosX = p->pos[0];
            p->obj->oPosY = p->pos[1];
            p->obj->oPosZ = p->pos[2];
            p->obj->header.gfx.pos[0] = p->pos[0];
            p->obj->header.gfx.pos[1] = p->pos[1];
            p->obj->header.gfx.pos[2] = p->pos[2];

            /* Hitbox for enemy damage */
            p->obj->hitboxRadius = 50.0f;
            p->obj->hitboxHeight = 50.0f;
            p->obj->hurtboxRadius = 50.0f;
            p->obj->hurtboxHeight = 50.0f;
            p->obj->oInteractType = INTERACT_FLAME;
            p->obj->oDamageOrCoinValue = 2;

            /* Scale: quick grow then stable */
            f32 scale = TF_WEAPON_SCALE;
            if (p->timer < 3) {
                scale = TF_WEAPON_SCALE * ((f32)p->timer / 3.0f);
            }
            p->obj->header.gfx.scale[0] = scale;
            p->obj->header.gfx.scale[1] = scale;
            p->obj->header.gfx.scale[2] = scale;

            /* Animate texture state (makes flame flicker) */
            p->obj->oAnimState = (p->obj->oAnimState + 1) % 10;

            /* Check if object was deleted externally (hit something) */
            if (p->obj->activeFlags == 0) {
                p->active = 0;
                p->obj = NULL;
            }
        }
    }
}

/* ── Fire a projectile ──────────────────────────────────────────── */

static void tf_fire_projectile(struct MarioState *m) {
    struct TfProjectile *p = tf_alloc_projectile();

    /* If recycling an active slot, clean up old object */
    if (p->active && p->obj != NULL) {
        obj_mark_for_deletion(p->obj);
    }

    /* Direction: camera look direction */
    s16 yawS16   = (s16)(gTFState.camera.yaw / 360.0f * 65536.0f);
    s16 pitchS16 = (s16)(gTFState.camera.pitch / 360.0f * 65536.0f);

    f32 dirX = coss(pitchS16) * sins(yawS16);
    f32 dirY = sins(pitchS16);
    f32 dirZ = coss(pitchS16) * coss(yawS16);

    /* Spawn position: in front of Mario at eye level */
    p->pos[0] = m->pos[0] + dirX * TF_WEAPON_SPAWN_FWD;
    p->pos[1] = m->pos[1] + 100.0f + dirY * TF_WEAPON_SPAWN_FWD;
    p->pos[2] = m->pos[2] + dirZ * TF_WEAPON_SPAWN_FWD;

    p->vel[0] = dirX * TF_WEAPON_SPEED;
    p->vel[1] = dirY * TF_WEAPON_SPEED;
    p->vel[2] = dirZ * TF_WEAPON_SPEED;

    p->timer = 0;
    p->active = 1;

    /* Spawn flame visual object */
    p->obj = spawn_object(m->marioObj, MODEL_RED_FLAME, bhvFlame);
    if (p->obj != NULL) {
        p->obj->oPosX = p->pos[0];
        p->obj->oPosY = p->pos[1];
        p->obj->oPosZ = p->pos[2];
        p->obj->oInteractType = INTERACT_FLAME;
        p->obj->oDamageOrCoinValue = 2;
        p->obj->header.gfx.scale[0] = TF_WEAPON_SCALE;
        p->obj->header.gfx.scale[1] = TF_WEAPON_SCALE;
        p->obj->header.gfx.scale[2] = TF_WEAPON_SCALE;
    }

    /* Muzzle sound */
    play_sound(SOUND_OBJ_FLAME_BLOWN, m->marioObj->header.gfx.cameraToObject);
}

/* ── Main weapon update (called from tf_movement.c) ─────────────── */

void tf_weapon_update(struct MarioState *m) {
    /* Tick cooldown */
    if (gTFState.shootCooldown > 0) {
        gTFState.shootCooldown--;
    }

    /* Fire on left mouse hold (auto-fire like L-STAR) */
    if (gTFState.mouseDown && gTFState.shootCooldown == 0 && gTFState.camera.captured) {
        gTFState.shootCooldown = TF_WEAPON_COOLDOWN;
        tf_fire_projectile(m);
        m->particleFlags |= PARTICLE_FIRE;
    }

    /* Update all live projectiles */
    tf_update_projectiles();
}
