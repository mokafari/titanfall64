#ifndef TF_HITSCAN_H
#define TF_HITSCAN_H

#include "types.h"

/* Hitscan weapon — secondary weapon (swap with mouse wheel or key) */

#define TF_HS_RANGE          3000.0f
#define TF_HS_DAMAGE           25
#define TF_HS_FIRE_RATE         4      /* frames between shots */
#define TF_HS_SPREAD_BASE       0.01f  /* radians */
#define TF_HS_SPREAD_AIR        0.03f
#define TF_HS_SPREAD_MOVE       0.02f
#define TF_HS_KICKBACK          2.0f   /* camera pitch kick (degrees) */
#define TF_HS_KICK_RECOVERY     0.4f

/* Enemy HP (SM64 enemies normally die in 1 hit) */
#define TF_HP_GOOMBA          25
#define TF_HP_KOOPA           50
#define TF_HP_BOBOMB          50
#define TF_HP_BULLY          100
#define TF_HP_PIRANHA         75
#define TF_HP_WHOMP          150
#define TF_HP_KING_BOBOMB    300
#define TF_HP_BOWSER         500
#define TF_HP_DEFAULT         50

#define TF_MAX_TRACKED_ENEMIES 240

struct HitscanState {
    s16 fireCooldown;
    f32 currentSpread;
    f32 pitchKickAccum;
    u16 totalKills;
    u16 levelKills;
    u8 hitMarkerTimer;
    u8 killMarkerTimer;
};

extern struct HitscanState gTFHitscan;

void tf_hitscan_init(void);
void tf_hitscan_update(struct MarioState *m);
void tf_hitscan_fire(struct MarioState *m);
s16  tf_hitscan_get_enemy_hp(s32 idx);
void tf_hitscan_reset_enemy_hp(void);

#endif
