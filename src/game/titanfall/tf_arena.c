/*
 * tf_arena.c — Wave-based arena mode
 *
 * Spawns enemy waves with increasing difficulty.
 * Tracks kills, survival time, speed stats.
 */

#include <math.h>
#include <string.h>
#include "sm64.h"
#include "engine/math_util.h"
#include "engine/behavior_script.h"
#include "engine/surface_collision.h"
#include "game/mario.h"
#include "game/object_helpers.h"
#include "game/object_list_processor.h"
#include "game/print.h"
#include "game/titanfall/tf_arena.h"
#include "game/titanfall/tf_movement.h"
#include "game/titanfall/tf_math.h"
#include "behavior_data.h"
#include "model_ids.h"
#include "sounds.h"
#include "audio/external.h"

struct ArenaState gTFArena = { 0 };

void tf_arena_init(void) {
    memset(&gTFArena, 0, sizeof(gTFArena));
    gTFArena.enabled = 1;
    gTFArena.lives = TF_ARENA_LIVES;
    gTFArena.waveTimer = TF_ARENA_WAVE_DELAY;
}

void tf_arena_on_enemy_killed(void) {
    if (!gTFArena.enabled) return;
    gTFArena.enemiesAlive--;
    if (gTFArena.enemiesAlive < 0) gTFArena.enemiesAlive = 0;
    gTFArena.totalKills++;
}

void tf_arena_update(struct MarioState *m, f32 dt) {
    if (!gTFArena.enabled || gTFArena.gameOver) return;

    gTFArena.survivalTime += dt;

    f32 speed = vec3f_magnitude_xz(m->vel);
    if (speed > gTFArena.topSpeed) gTFArena.topSpeed = speed;

    /* Wave complete check */
    if (gTFArena.enemiesThisWave > 0 && gTFArena.enemiesAlive <= 0) {
        if (!gTFArena.waveCleared) {
            gTFArena.waveCleared = 1;
            gTFArena.waveTimer = TF_ARENA_WAVE_DELAY;
        }
    }

    /* Wave countdown */
    if (gTFArena.waveCleared || gTFArena.currentWave == 0) {
        gTFArena.waveTimer--;
        if (gTFArena.waveTimer <= 0) {
            tf_arena_spawn_wave(m);
        }
    }

    /* Death check: fell off level */
    if (m->pos[1] < -5000.0f) {
        gTFArena.lives--;
        if (gTFArena.lives <= 0) {
            gTFArena.gameOver = 1;
        } else {
            /* Respawn at level start area (approximate) */
            m->pos[0] = 0.0f;
            m->pos[1] = 500.0f;
            m->pos[2] = 0.0f;
            m->vel[0] = 0.0f;
            m->vel[1] = 0.0f;
            m->vel[2] = 0.0f;
        }
    }
}

void tf_arena_spawn_wave(struct MarioState *m) {
    gTFArena.currentWave++;
    gTFArena.waveCleared = 0;

    s16 numEnemies = TF_ARENA_BASE_ENEMIES + (gTFArena.currentWave - 1) * TF_ARENA_ENEMIES_PER_WAVE;
    if (numEnemies > TF_ARENA_MAX_ENEMIES) numEnemies = TF_ARENA_MAX_ENEMIES;

    gTFArena.enemiesThisWave = numEnemies;
    gTFArena.enemiesAlive = numEnemies;

    for (s16 i = 0; i < numEnemies; i++) {
        f32 angle = (f32)i / (f32)numEnemies * 2.0f * 3.14159f;
        f32 radius = TF_ARENA_SPAWN_MIN_DIST +
                     (random_float() * (TF_ARENA_SPAWN_RADIUS - TF_ARENA_SPAWN_MIN_DIST));

        f32 spawnX = m->pos[0] + cosf(angle) * radius;
        f32 spawnZ = m->pos[2] + sinf(angle) * radius;

        struct Surface *floor = NULL;
        f32 spawnY = find_floor(spawnX, m->pos[1] + 500.0f, spawnZ, &floor);
        if (floor == NULL) spawnY = m->pos[1];

        /* Pick enemy type based on wave */
        s32 model;
        const BehaviorScript *behavior;
        f32 roll = random_float();

        if (gTFArena.currentWave < 6 || roll < 0.4f) {
            model = MODEL_GOOMBA;
            behavior = bhvGoomba;
        } else if (gTFArena.currentWave < 11 || roll < 0.65f) {
            model = MODEL_KOOPA_WITHOUT_SHELL;
            behavior = bhvKoopa;
        } else if (gTFArena.currentWave < 16 || roll < 0.8f) {
            model = MODEL_BLACK_BOBOMB;
            behavior = bhvBobomb;
        } else {
            model = MODEL_BULLY;
            behavior = bhvSmallBully;
        }

        struct Object *enemy = spawn_object(m->marioObj, model, behavior);
        if (enemy != NULL) {
            enemy->oPosX = spawnX;
            enemy->oPosY = spawnY + 50.0f;
            enemy->oPosZ = spawnZ;
            enemy->oHomeX = spawnX;
            enemy->oHomeY = spawnY + 50.0f;
            enemy->oHomeZ = spawnZ;
        }
    }

    play_sound(SOUND_GENERAL_COLLECT_1UP, gGlobalSoundSource);
}

void tf_arena_render_hud(void) {
    if (!gTFArena.enabled) return;

    if (gTFArena.gameOver) {
        print_text(100, 100, "GAME OVER");
        print_text_fmt_int(90, 120, "WAVE %d", gTFArena.currentWave);
        print_text_fmt_int(90, 134, "KILLS %d", gTFArena.totalKills);
        print_text_fmt_int(90, 148, "TOP SPEED %d", (s32)gTFArena.topSpeed);
        s32 secs = (s32)gTFArena.survivalTime;
        print_text_fmt_int(90, 162, "TIME %d", secs);
        return;
    }

    /* Wave number */
    print_text_fmt_int(120, 20, "WAVE %d", gTFArena.currentWave);

    /* Lives */
    print_text_fmt_int(20, 20, "x%d", gTFArena.lives);

    /* Enemies remaining */
    print_text_fmt_int(240, 20, "%d", gTFArena.enemiesAlive);

    /* Wave incoming countdown */
    if ((gTFArena.waveCleared || gTFArena.currentWave == 0) && gTFArena.waveTimer > 0) {
        s32 countdown = gTFArena.waveTimer / 30 + 1;
        print_text_fmt_int(110, 60, "NEXT WAVE %d", countdown);
    }
}
