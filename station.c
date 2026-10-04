#include "station.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const GameDef* const station_games[GAME_COUNT] = {
    &game_shooter,
    &game_snake,
    &game_breakout,
    &game_flappy,
    &game_dino,
    &game_pong,
    &game_tetris,
    &game_2048,
    &game_mines,
    &game_simon,
    &game_whack,
    &game_reaction,
};

#define MENU_ROWS  4
#define MENU_TOP   12
#define MENU_ROW_H 13

void station_init(Station* st, uint32_t seed, const uint32_t* best) {
    memset(st, 0, sizeof(Station));
    st->ctx.rng = seed ? seed : 0x1234567u;
    if(best) memcpy(st->best, best, sizeof(st->best));
    st->state = StationMenu;
}

static void end_game(Station* st) {
    if(st->gs) {
        free(st->gs);
        st->gs = NULL;
    }
    st->game = NULL;
}

void station_free(Station* st) {
    end_game(st);
}

static void collect(Station* st) {
    st->sfx |= st->ctx.sfx;
    if(st->ctx.tone) st->tone = st->ctx.tone;
    st->ctx.sfx = 0;
    st->ctx.tone = 0;
}

static void start_game(Station* st, uint32_t now_ms) {
    const GameDef* def = station_games[st->sel];
    if(!st->gs || st->game != def) {
        end_game(st);
        st->gs = malloc(def->state_size);
    }
    st->game = def;
    memset(st->gs, 0, def->state_size);
    uint32_t rng = st->ctx.rng;
    memset(&st->ctx, 0, sizeof(GameCtx));
    st->ctx.rng = rng;
    st->ctx.now_ms = now_ms;
    st->ctx.best = st->best[st->sel];
    st->new_best = false;
    def->init(st->gs, &st->ctx);
    collect(st);
    st->state = StationPlaying;
}

static void check_over(Station* st) {
    if(!st->ctx.over || st->state != StationPlaying) return;
    st->state = StationOver;
    st->ctx.held = 0;
    if(!(st->ctx.sfx & (SFX_GAME_OVER | SFX_WIN))) {
        st->ctx.sfx |= st->ctx.won ? SFX_WIN : SFX_GAME_OVER;
    }
    uint32_t score = st->ctx.score > 0 ? (uint32_t)st->ctx.score : 0;
    uint32_t* best = &st->best[st->sel];
    bool better;
    if(st->game->lower_is_better) {
        better = st->ctx.won && score > 0 && (*best == 0 || score < *best);
    } else {
        better = score > *best;
    }
    if(better) {
        *best = score;
        st->new_best = true;
        st->save_pending = true;
    }
}

void station_tick(Station* st, uint32_t now_ms) {
    st->frame++;
    if(st->state == StationPlaying) {
        st->ctx.now_ms = now_ms;
        st->game->update(st->gs, &st->ctx);
        check_over(st);
    }
    collect(st);
}

bool station_input(Station* st, InputKey key, InputType type, uint32_t now_ms) {
    if(key == InputKeyBack && type == InputTypeLong) return false;

    /* keep the held-keys mask in sync whatever the state */
    if(key != InputKeyBack) {
        if(type == InputTypePress) st->ctx.held |= KEY_BIT(key);
        if(type == InputTypeRelease) st->ctx.held &= ~KEY_BIT(key);
    }

    switch(st->state) {
    case StationMenu:
        if(type == InputTypePress || type == InputTypeRepeat) {
            if(key == InputKeyUp) {
                st->sel = (st->sel + GAME_COUNT - 1) % GAME_COUNT;
                st->sfx |= SFX_TICK;
            } else if(key == InputKeyDown) {
                st->sel = (st->sel + 1) % GAME_COUNT;
                st->sfx |= SFX_TICK;
            }
            if(st->sel < st->scroll) st->scroll = st->sel;
            if(st->sel >= st->scroll + MENU_ROWS) st->scroll = st->sel - MENU_ROWS + 1;
        }
        if(key == InputKeyOk && type == InputTypeShort) {
            st->ctx.held = 0;
            start_game(st, now_ms);
        }
        if(key == InputKeyBack && type == InputTypeShort) return false;
        break;

    case StationPlaying:
        if(key == InputKeyBack) {
            if(type == InputTypeShort) {
                st->ctx.held = 0;
                st->state = StationPaused;
            }
            break;
        }
        st->ctx.now_ms = now_ms;
        st->game->input(st->gs, &st->ctx, key, type);
        check_over(st);
        collect(st);
        break;

    case StationPaused:
        if(key == InputKeyOk && type == InputTypeShort) st->state = StationPlaying;
        if(key == InputKeyBack && type == InputTypeShort) {
            end_game(st);
            st->state = StationMenu;
        }
        break;

    case StationOver:
        if(key == InputKeyOk && type == InputTypeShort) start_game(st, now_ms);
        if(key == InputKeyBack && type == InputTypeShort) {
            end_game(st);
            st->state = StationMenu;
        }
        break;
    }
    return true;
}

/* ---------- drawing ---------- */

static void format_score(const GameDef* def, uint32_t v, char* buf, size_t n) {
    if(def->unit) {
        snprintf(buf, n, "%lu %s", (unsigned long)v, def->unit);
    } else {
        snprintf(buf, n, "%lu", (unsigned long)v);
    }
}

static void draw_menu(Station* st, Canvas* c) {
    canvas_draw_box(c, 0, 0, SCREEN_W, 11);
    canvas_set_color(c, ColorWhite);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, SCREEN_W / 2, 9, AlignCenter, AlignBottom, "GAME STATION");
    canvas_set_color(c, ColorBlack);

    canvas_set_font(c, FontSecondary);
    char buf[24];
    for(int i = 0; i < MENU_ROWS; i++) {
        int idx = st->scroll + i;
        if(idx >= GAME_COUNT) break;
        const GameDef* def = station_games[idx];
        int y = MENU_TOP + i * MENU_ROW_H;
        bool sel = idx == st->sel;
        if(sel) {
            canvas_draw_rbox(c, 0, y, SCREEN_W - 5, MENU_ROW_H - 1, 2);
            canvas_set_color(c, ColorWhite);
        }
        draw_sprite(c, 3, y + 2 + (7 - def->icon->h) / 2, def->icon, 0);
        canvas_draw_str(c, 16, y + 9, def->name);
        if(st->best[idx]) {
            format_score(def, st->best[idx], buf, sizeof(buf));
            canvas_draw_str_aligned(c, SCREEN_W - 8, y + 9, AlignRight, AlignBottom, buf);
        }
        canvas_set_color(c, ColorBlack);
    }

    /* scrollbar */
    int track = SCREEN_H - MENU_TOP;
    int thumb = track * MENU_ROWS / GAME_COUNT;
    int ty = MENU_TOP + (track - thumb) * st->scroll / (GAME_COUNT - MENU_ROWS);
    canvas_draw_line(c, SCREEN_W - 2, MENU_TOP, SCREEN_W - 2, SCREEN_H - 1);
    canvas_draw_box(c, SCREEN_W - 3, ty, 3, thumb);
}

static void draw_panel(Canvas* c, int w, int h) {
    int x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2;
    canvas_set_color(c, ColorWhite);
    canvas_draw_box(c, x - 1, y - 1, w + 2, h + 2);
    canvas_set_color(c, ColorBlack);
    canvas_draw_rframe(c, x, y, w, h, 3);
}

void station_draw(Station* st, Canvas* c) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontSecondary);

    if(st->state == StationMenu) {
        draw_menu(st, c);
        return;
    }

    st->game->draw(st->gs, c, &st->ctx);
    canvas_set_color(c, ColorBlack);

    char buf[40];
    if(st->state == StationPaused) {
        draw_panel(c, 96, 36);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, SCREEN_W / 2, 24, AlignCenter, AlignBottom, "PAUSED");
        canvas_set_font(c, FontSecondary);
        canvas_draw_str_aligned(c, SCREEN_W / 2, 35, AlignCenter, AlignBottom, "OK: resume");
        canvas_draw_str_aligned(c, SCREEN_W / 2, 45, AlignCenter, AlignBottom, "Back: menu");
    } else if(st->state == StationOver) {
        const GameDef* def = st->game;
        draw_panel(c, 120, 46);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(
            c, SCREEN_W / 2, 20, AlignCenter, AlignBottom, st->ctx.won ? "YOU WIN!" : "GAME OVER");
        canvas_set_font(c, FontSecondary);
        char sc[24];
        format_score(def, st->ctx.score > 0 ? (uint32_t)st->ctx.score : 0, sc, sizeof(sc));
        snprintf(buf, sizeof(buf), "Score: %s", sc);
        canvas_draw_str_aligned(c, SCREEN_W / 2, 31, AlignCenter, AlignBottom, buf);
        if(st->new_best) {
            if((st->frame / 8) % 2) {
                canvas_draw_str_aligned(c, SCREEN_W / 2, 41, AlignCenter, AlignBottom, "NEW BEST!");
            }
        } else if(st->best[st->sel]) {
            format_score(def, st->best[st->sel], sc, sizeof(sc));
            snprintf(buf, sizeof(buf), "Best: %s", sc);
            canvas_draw_str_aligned(c, SCREEN_W / 2, 41, AlignCenter, AlignBottom, buf);
        }
        canvas_draw_str_aligned(c, SCREEN_W / 2, 52, AlignCenter, AlignBottom, "OK: retry   Back: menu");
    }
}
