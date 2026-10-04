#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef HOST_TEST
#include "test/host_shim.h"
#else
#include <gui/canvas.h>
#include <input/input.h>
#endif

#define SCREEN_W 128
#define SCREEN_H 64
#define HUD_H    9

#define MAX_BULLETS   16
#define MAX_EBULLETS  32
#define MAX_ENEMIES   16
#define MAX_PARTICLES 64
#define MAX_POWERUPS  4
#define NUM_STARS     24
#define MAX_LIVES     5

typedef enum {
    StateTitle,
    StatePlaying,
    StatePaused,
    StateGameOver,
} GameState;

typedef enum {
    EnemyDrifter,
    EnemyShooter,
    EnemyDiver,
    EnemyBoss,
} EnemyType;

typedef enum {
    PowerDouble,
    PowerShield,
    PowerLife,
} PowerType;

/* Sound/vibration events raised by game_update, played by the app glue */
#define SFX_KILL      (1 << 0)
#define SFX_HIT       (1 << 1)
#define SFX_POWER     (1 << 2)
#define SFX_BOSS      (1 << 3)
#define SFX_GAME_OVER (1 << 4)
#define SFX_BOSS_KILL (1 << 5)

typedef struct {
    float x, y, vx, vy;
    bool active;
} Bullet;

typedef struct {
    float x, y, vx, vy;
    int8_t life;
} Particle;

typedef struct {
    EnemyType type;
    float x, y; /* center x, top y */
    float vx, vy;
    float base_x, target_y;
    int t;
    int16_t hp, max_hp;
    int16_t fire_cd;
    uint8_t flash;
    bool active;
} Enemy;

typedef struct {
    PowerType type;
    float x, y;
    bool active;
} PowerUp;

typedef struct {
    float x, y, speed;
} Star;

typedef struct {
    GameState state;
    uint32_t frame;
    uint32_t rng;

    float px, py; /* ship center x, top y */
    int lives;
    uint32_t score;
    uint32_t high_score;
    bool new_high;
    bool save_pending;

    int wave;
    int to_spawn;
    int spawn_timer;
    int banner_timer;
    int fire_cd;
    int double_timer;
    int shield_timer;
    int invuln_timer;
    int shake;

    bool held_left, held_right, held_up, held_down, held_ok;

    Bullet bullets[MAX_BULLETS];
    Bullet ebullets[MAX_EBULLETS];
    Enemy enemies[MAX_ENEMIES];
    Particle particles[MAX_PARTICLES];
    PowerUp powerups[MAX_POWERUPS];
    Star stars[NUM_STARS];

    uint8_t sfx;
} Game;

void game_init(Game* g, uint32_t seed, uint32_t high_score);
void game_start(Game* g);
void game_update(Game* g);
void game_draw(const Game* g, Canvas* canvas);
/* Returns false when the app should exit */
bool game_input(Game* g, InputKey key, InputType type);
