#pragma once

#include "common.h"

#define GAME_COUNT 12

typedef enum {
    StationMenu,
    StationPlaying,
    StationPaused,
    StationOver,
} StationState;

typedef struct {
    StationState state;
    uint32_t frame;
    int sel, scroll;

    const GameDef* game;
    void* gs; /* running game's state */
    GameCtx ctx;

    uint32_t best[GAME_COUNT];
    bool new_best;
    bool save_pending;

    uint8_t sfx, tone; /* collected for the app glue to play */
} Station;

extern const GameDef* const station_games[GAME_COUNT];

void station_init(Station* st, uint32_t seed, const uint32_t* best);
void station_free(Station* st);
void station_tick(Station* st, uint32_t now_ms);
void station_draw(Station* st, Canvas* canvas);
/* Returns false when the app should exit */
bool station_input(Station* st, InputKey key, InputType type, uint32_t now_ms);
