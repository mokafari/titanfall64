/*
 * tf_hitscan.c — Hitscan weapon (secondary to L-STAR projectile weapon)
 *
 * Raycast from camera center, damages enemies with HP pools.
 * Swap between weapons with mouse wheel or 1/2 keys.
 */

#include <math.h>
#include "sm64.h"
#include "engine/math_util.h"
#include "engine/behavior_script.h"
#include "game/mario.h"
#include "game/interaction.h"
#include "game/object_helpers.h"
#include "game/object_list_processor.h"
#include "game/titanfall/tf_hitscan.h"
#include "game/titanfall/tf_movement.h"
#include "game/titanfall/tf_math.h"
#include "game/titanfall/tf_arena.h"
#include "behavior_data.h"
#include "object_fields.h"
#include "sounds.h"
#include "audio/external.h"

struct HitscanState gTFHitscan = { 0 };

/* ── Enemy HP tracking ─────────────────────────────────────────── */

static s16 sEnemyHP[TF_MAX_TRACKED_ENEMIES];
static u8 sEnemyHPInit[TF_MAX_TRACKED_ENEMIES];

extern struct ObjectNode *gObjectLists;
extern struct Object gObjectPool[];

static s32 get_obj_index(struct Object *obj) {
    return (s32)(obj - &gObjectPool[0]);
}

static s16 get_enemy_max_hp(struct Object *obj) {
    const BehaviorScript *b = obj->behavior;
    if (b == bhvGoomba)       return TF_HP_GOOMBA;
    if (b == bhvKoopa)        return TF_HP_KOOPA;
    if (b == bhvBobomb)       return TF_HP_BOBOMB;
    if (b == bhvSmallBully)   return TF_HP_BULLY;
    if (b == bhvBigBully)     return TF_HP_BULLY * 2;
    if (b == bhvPiranhaPlant) return TF_HP_PIRANHA;
    if (b == bhvKingBobomb)   return TF_HP_KING_BOBOMB;
    if (b == bhvBowser)       return TF_HP_BOWSER;
    return TF_HP_DEFAULT;
}

/* ── Init ──────────────────────────────────────────────────────── */

void tf_hitscan_init(void) {
    memset(&gTFHitscan, 0, sizeof(gTFHitscan));
    tf_hitscan_reset_enemy_hp();
}

void tf_hitscan_reset_enemy_hp(void) {
    memset(sEnemyHP, 0, sizeof(sEnemyHP));
    memset(sEnemyHPInit, 0, sizeof(sEnemyHPInit));
}

s16 tf_hitscan_get_enemy_hp(s32 idx) {
    if (idx < 0 || idx >= TF_MAX_TRACKED_ENEMIES) return 0;
    return sEnemyHP[idx];
}

/* ── Per-frame update ──────────────────────────────────────────── */

void tf_hitscan_update(struct MarioState *m) {
    if (gTFHitscan.fireCooldown > 0) gTFHitscan.fireCooldown--;
    if (gTFHitscan.hitMarkerTimer > 0) gTFHitscan.hitMarkerTimer--;
    if (gTFHitscan.killMarkerTimer > 0) gTFHitscan.killMarkerTimer--;

    /* Spread calculation */
    f32 targetSpread = TF_HS_SPREAD_BASE;
    f32 hspeed = vec3f_magnitude_xz(m->vel);
    targetSpread += (hspeed / TF_SLIDE_MAX_SPEED) * TF_HS_SPREAD_MOVE;
    if (m->pos[1] > m->floorHeight + 10.0f) {
        targetSpread += TF_HS_SPREAD_AIR;
    }
    gTFHitscan.currentSpread += (targetSpread - gTFHitscan.currentSpread) * 0.2f;

    /* Recoil recovery */
    if (gTFHitscan.pitchKickAccum > 0.1f) {
        f32 recovery = gTFHitscan.pitchKickAccum * TF_HS_KICK_RECOVERY;
        gTFHitscan.pitchKickAccum -= recovery;
        gTFState.camera.pitch -= recovery;
    }

    /* Fire on mouse click */
    if (gTFState.mouseDown && gTFHitscan.fireCooldown <= 0 && gTFState.camera.captured) {
        tf_hitscan_fire(m);
        gTFHitscan.fireCooldown = TF_HS_FIRE_RATE;
    }
}

/* ── Ray-sphere intersection test ──────────────────────────────── */

static s32 ray_hits_sphere(Vec3f rayOrigin, Vec3f rayDir, Vec3f center, f32 radius, f32 *outDist) {
    f32 ox = rayOrigin[0] - center[0];
    f32 oy = rayOrigin[1] - center[1];
    f32 oz = rayOrigin[2] - center[2];

    f32 b = 2.0f * (ox * rayDir[0] + oy * rayDir[1] + oz * rayDir[2]);
    f32 c = ox * ox + oy * oy + oz * oz - radius * radius;
    f32 disc = b * b - 4.0f * c;

    if (disc < 0.0f) return 0;

    f32 sqrtDisc = sqrtf(disc);
    f32 t = (-b - sqrtDisc) * 0.5f;
    if (t < 0.0f) t = (-b + sqrtDisc) * 0.5f;
    if (t < 0.0f) return 0;

    *outDist = t;
    return 1;
}

/* ── Fire hitscan ──────────────────────────────────────────────── */

void tf_hitscan_fire(struct MarioState *m) {
    Vec3f rayOrigin;
    rayOrigin[0] = gTFState.camera.pos[0];
    rayOrigin[1] = gTFState.camera.pos[1];
    rayOrigin[2] = gTFState.camera.pos[2];

    /* Ray direction from camera with spread */
    s16 yawS16 = (s16)(gTFState.camera.yaw / 360.0f * 65536.0f);
    s16 pitchS16 = (s16)(gTFState.camera.pitch / 360.0f * 65536.0f);

    f32 spreadX = (random_float() - 0.5f) * 2.0f * gTFHitscan.currentSpread;
    f32 spreadY = (random_float() - 0.5f) * 2.0f * gTFHitscan.currentSpread;

    s16 yawSpread = (s16)(spreadX / (2.0f * 3.14159f) * 65536.0f);
    s16 pitchSpread = (s16)(spreadY / (2.0f * 3.14159f) * 65536.0f);

    Vec3f rayDir;
    rayDir[0] = coss(pitchS16 + pitchSpread) * sins(yawS16 + yawSpread);
    rayDir[1] = -sins(pitchS16 + pitchSpread);
    rayDir[2] = coss(pitchS16 + pitchSpread) * coss(yawS16 + yawSpread);

    /* Find closest enemy hit */
    struct Object *hitObj = NULL;
    f32 hitDist = TF_HS_RANGE;

    if (gObjectLists == NULL) goto no_hit;

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
                Vec3f objPos = { obj->oPosX, obj->oPosY + 40.0f, obj->oPosZ };
                f32 dist;
                if (ray_hits_sphere(rayOrigin, rayDir, objPos, 80.0f, &dist)) {
                    if (dist < hitDist) {
                        hitDist = dist;
                        hitObj = obj;
                    }
                }
            }

            node = next;
        }
    }

no_hit:
    if (hitObj != NULL) {
        s32 idx = get_obj_index(hitObj);
        if (idx >= 0 && idx < TF_MAX_TRACKED_ENEMIES) {
            /* Initialize HP on first hit */
            if (!sEnemyHPInit[idx]) {
                sEnemyHP[idx] = get_enemy_max_hp(hitObj);
                sEnemyHPInit[idx] = 1;
            }

            sEnemyHP[idx] -= TF_HS_DAMAGE;
            gTFHitscan.hitMarkerTimer = 6;

            if (sEnemyHP[idx] <= 0) {
                /* Kill */
                hitObj->oInteractStatus |= INT_STATUS_WAS_ATTACKED
                                         | INT_STATUS_INTERACTED
                                         | ATTACK_PUNCH;
                sEnemyHPInit[idx] = 0;
                gTFHitscan.killMarkerTimer = 15;
                gTFHitscan.totalKills++;
                gTFHitscan.levelKills++;
                tf_arena_on_enemy_killed();
            }
        }
    }

    /* Recoil */
    gTFHitscan.pitchKickAccum += TF_HS_KICKBACK;
    gTFState.camera.pitch += TF_HS_KICKBACK;
    gTFHitscan.currentSpread += 0.005f;

    /* Sound */
    play_sound(SOUND_OBJ_CANNON1, m->marioObj->header.gfx.cameraToObject);
}
