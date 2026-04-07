#ifndef TF_ARENA_H
#define TF_ARENA_H

#include "types.h"

#define TF_ARENA_WAVE_DELAY       150
#define TF_ARENA_BASE_ENEMIES       3
#define TF_ARENA_ENEMIES_PER_WAVE   2
#define TF_ARENA_MAX_ENEMIES       20
#define TF_ARENA_SPAWN_RADIUS    1500.0f
#define TF_ARENA_SPAWN_MIN_DIST   500.0f
#define TF_ARENA_LIVES              3

struct ArenaState {
    u8 enabled;
    u8 gameOver;
    s16 currentWave;
    s16 waveTimer;
    s16 enemiesAlive;
    s16 enemiesThisWave;
    s16 totalKills;
    s16 lives;
    f32 survivalTime;
    f32 topSpeed;
    u16 bestChain;
    u8 waveCleared;
};

extern struct ArenaState gTFArena;

void tf_arena_init(void);
void tf_arena_update(struct MarioState *m, f32 dt);
void tf_arena_spawn_wave(struct MarioState *m);
void tf_arena_on_enemy_killed(void);
void tf_arena_render_hud(void);

#endif
