#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef HOST_TEST
#include <stdio.h>
#include <string.h>
#include "test/host_shim.h"
#else
#include <gui/canvas.h>
#include <input/input.h>
#endif

#define SCREEN_W 128
#define SCREEN_H 64

/* ---------- sprites: rows of '#' (ink) and '.' (blank) ---------- */

typedef struct {
    uint8_t w, h;
    const char* const* rows;
} Sprite;

void draw_sprite(Canvas* c, int x, int y, const Sprite* s, int min_y);
/* 3x5 pixel digits, handy where fonts are too big */
void draw_tiny_number(Canvas* c, int x, int y, int n);
/* Text with a white box behind it so it reads over the playfield */
void draw_str_boxed(Canvas* c, int x, int y, Align h, const char* str);
void draw_hearts(Canvas* c, int right_x, int y, int count);

/* ---------- sound / vibration requests ---------- */

#define SFX_BLIP      (1 << 0) /* short click: hit, pickup, eat */
#define SFX_HIT       (1 << 1) /* you got hurt: vibro + red */
#define SFX_POWER     (1 << 2) /* arpeggio: power-up, level up */
#define SFX_ALARM     (1 << 3) /* boss incoming */
#define SFX_GAME_OVER (1 << 4)
#define SFX_WIN       (1 << 5)
#define SFX_TICK      (1 << 6) /* menu move */

#define KEY_BIT(k) (1u << (k))

/* What the framework shares with the running game */
typedef struct {
    uint32_t rng;
    uint32_t now_ms; /* system tick in ms, fresh for every call */
    uint32_t best; /* best score so far for this game (for HUDs) */
    uint8_t held; /* KEY_BIT() mask of keys currently held */
    uint8_t sfx; /* SFX_* flags to play after this call */
    uint8_t tone; /* 1..4: play a Simon pad tone */
    int32_t score;
    bool over; /* set by the game: shows the game over screen */
    bool won; /* game ended in victory */
} GameCtx;

uint32_t rnd(GameCtx* ctx);
float rndf(GameCtx* ctx);
int rndi(GameCtx* ctx, int n);
float clampf(float v, float lo, float hi);
int clampi(int v, int lo, int hi);
bool overlap(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh);

typedef struct {
    const char* name;
    const Sprite* icon; /* at most 9x7 */
    size_t state_size;
    bool lower_is_better; /* e.g. reaction time */
    const char* unit; /* shown after the score, or NULL */
    void (*init)(void* state, GameCtx* ctx);
    void (*update)(void* state, GameCtx* ctx); /* 30 times per second */
    void (*draw)(void* state, Canvas* canvas, GameCtx* ctx);
    /* every key event except Back, which the framework owns */
    void (*input)(void* state, GameCtx* ctx, InputKey key, InputType type);
} GameDef;

extern const GameDef game_shooter;
extern const GameDef game_snake;
extern const GameDef game_breakout;
extern const GameDef game_flappy;
extern const GameDef game_dino;
extern const GameDef game_pong;
extern const GameDef game_2048;
extern const GameDef game_mines;
extern const GameDef game_tetris;
extern const GameDef game_simon;
extern const GameDef game_reaction;
extern const GameDef game_whack;
