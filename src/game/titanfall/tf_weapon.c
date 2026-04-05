/*
 * tf_weapon.c — L-STAR style energy projectile weapon
 *
 * Left mouse fires slow red flame projectiles in camera direction.
 * Damages enemies by directly setting oInteractStatus (INT_STATUS_WAS_ATTACKED)
 * on any enemy object within hit radius — same as a punch/kick.
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
#define TF_PROJ_HIT_RADIUS  80.0f  /* distance to damage enemies */

struct TfProjectile {
    struct Object *obj;
    Vec3f pos;
    Vec3f vel;
    s16 timer;
    u8 active;
};

static struct TfProjectile sProjectiles[TF_MAX_PROJECTILES] = { 0 };

static struct TfProjectile *tf_alloc_projectile(void) {
    for (s32 i = 0; i < TF_MAX_PROJECTILES; i++) {
        if (!sProjectiles[i].active) return &sProjectiles[i];
    }
    return &sProjectiles[0];  /* recycle oldest */
}

/* ── Check if projectile hits any enemy ─────────────────────────── */

static void tf_projectile_check_enemy_hits(struct TfProjectile *p) {
    if (gObjectLists == NULL) return;

    for (s32 listIdx = 0; listIdx < 2; listIdx++) {
        s32 list = (listIdx == 0) ? OBJ_LIST_GENACTOR : OBJ_LIST_DESTRUCTIVE;
        struct ObjectNode *listHead = &gObjectLists[list];
        if (listHead == NULL) continue;

        struct ObjectNode *node = listHead->next;
        s32 safety = 0;

        while (node != listHead && safety < 256) {
            safety++;
            struct Object *obj = (struct Object *)node;
            struct ObjectNode *next = node->next;

            if (obj != NULL && obj->activeFlags != 0
                && (struct Object *)obj != gMarioObject) {
                f32 dx = p->pos[0] - obj->oPosX;
                f32 dy = p->pos[1] - obj->oPosY;
                f32 dz = p->pos[2] - obj->oPosZ;
                f32 distSq = dx * dx + dy * dy + dz * dz;

                if (distSq < TF_PROJ_HIT_RADIUS * TF_PROJ_HIT_RADIUS) {
                    obj->oInteractStatus |= INT_STATUS_WAS_ATTACKED
                                          | INT_STATUS_INTERACTED
                                          | ATTACK_PUNCH;

                    if (p->obj != NULL) {
                        obj_mark_for_deletion(p->obj);
                    }
                    p->active = 0;
                    p->obj = NULL;
                    return;
                }
            }

            node = next;
        }
    }
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
        p->vel[1] -= 0.3f;

        /* Lifetime */
        if (p->timer > TF_WEAPON_LIFETIME) {
            if (p->obj != NULL) obj_mark_for_deletion(p->obj);
            p->active = 0;
            p->obj = NULL;
            continue;
        }

        /* Floor collision */
        struct Surface *floor = NULL;
        f32 floorY = find_floor(p->pos[0], p->pos[1] + 50.0f, p->pos[2], &floor);
        if (floor != NULL && p->pos[1] < floorY + 15.0f) {
            if (p->obj != NULL) obj_mark_for_deletion(p->obj);
            p->active = 0;
            p->obj = NULL;
            continue;
        }

        /* Check enemy hits */
        tf_projectile_check_enemy_hits(p);
        if (!p->active) continue;  /* was destroyed by hit */

        /* Update SM64 object visuals */
        if (p->obj != NULL) {
            p->obj->oPosX = p->pos[0];
            p->obj->oPosY = p->pos[1];
            p->obj->oPosZ = p->pos[2];

            f32 scale = TF_WEAPON_SCALE;
            if (p->timer < 3) scale *= (f32)p->timer / 3.0f;
            /* Don't use cur_obj_scale — it needs gCurrentObject set.
             * Set scale directly on the graphics node. */
            p->obj->header.gfx.scale[0] = scale;
            p->obj->header.gfx.scale[1] = scale;
            p->obj->header.gfx.scale[2] = scale;

            if (p->obj->activeFlags == 0) {
                p->active = 0;
                p->obj = NULL;
            }
        }
    }
}

/* ── Fire ───────────────────────────────────────────────────────── */

static void tf_fire_projectile(struct MarioState *m) {
    struct TfProjectile *p = tf_alloc_projectile();

    if (p->active && p->obj != NULL) {
        obj_mark_for_deletion(p->obj);
    }

    s16 yawS16   = (s16)(gTFState.camera.yaw / 360.0f * 65536.0f);
    s16 pitchS16 = (s16)(gTFState.camera.pitch / 360.0f * 65536.0f);

    f32 dirX = coss(pitchS16) * sins(yawS16);
    f32 dirY = sins(pitchS16);
    f32 dirZ = coss(pitchS16) * coss(yawS16);

    p->pos[0] = m->pos[0] + dirX * TF_WEAPON_SPAWN_FWD;
    p->pos[1] = m->pos[1] + 100.0f + dirY * TF_WEAPON_SPAWN_FWD;
    p->pos[2] = m->pos[2] + dirZ * TF_WEAPON_SPAWN_FWD;

    p->vel[0] = dirX * TF_WEAPON_SPEED;
    p->vel[1] = dirY * TF_WEAPON_SPEED;
    p->vel[2] = dirZ * TF_WEAPON_SPEED;

    p->timer = 0;
    p->active = 1;

    p->obj = spawn_object(m->marioObj, MODEL_RED_FLAME, bhvFlame);
    if (p->obj != NULL) {
        p->obj->oPosX = p->pos[0];
        p->obj->oPosY = p->pos[1];
        p->obj->oPosZ = p->pos[2];
        p->obj->header.gfx.scale[0] = TF_WEAPON_SCALE;
        p->obj->header.gfx.scale[1] = TF_WEAPON_SCALE;
        p->obj->header.gfx.scale[2] = TF_WEAPON_SCALE;
    }

    play_sound(SOUND_OBJ_FLAME_BLOWN, m->marioObj->header.gfx.cameraToObject);
}

/* ── Main weapon update ─────────────────────────────────────────── */

void tf_weapon_update(struct MarioState *m) {
    if (gTFState.shootCooldown > 0) {
        gTFState.shootCooldown--;
    }

    if (gTFState.mouseDown && gTFState.shootCooldown == 0 && gTFState.camera.captured) {
        gTFState.shootCooldown = TF_WEAPON_COOLDOWN;
        tf_fire_projectile(m);
        m->particleFlags |= PARTICLE_FIRE;
    }

    tf_update_projectiles();
}

/* Unused — behavior calls this but we manage projectiles ourselves */
void tf_projectile_update(void) { }
