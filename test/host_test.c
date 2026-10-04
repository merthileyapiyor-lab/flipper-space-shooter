/* PC test harness: runs game.c with a bot, checks invariants, dumps frames as PGM. */
#include "../game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- canvas shim ---- */

static void put(Canvas* c, int x, int y) {
    if(x < 0 || y < 0 || x >= 128 || y >= 64) return;
    c->px[y][x] = c->color == ColorBlack;
}
void canvas_clear(Canvas* c) {
    memset(c->px, 0, sizeof(c->px));
}
void canvas_set_color(Canvas* c, Color color) {
    c->color = color;
}
void canvas_set_font(Canvas* c, Font font) {
    c->font = font;
}
void canvas_draw_dot(Canvas* c, int32_t x, int32_t y) {
    put(c, x, y);
}
void canvas_draw_line(Canvas* c, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    int dx = abs(x2 - x1), dy = -abs(y2 - y1), sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    for(;;) {
        put(c, x1, y1);
        if(x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if(e2 >= dy) err += dy, x1 += sx;
        if(e2 <= dx) err += dx, y1 += sy;
    }
}
void canvas_draw_box(Canvas* c, int32_t x, int32_t y, size_t w, size_t h) {
    for(size_t j = 0; j < h; j++)
        for(size_t i = 0; i < w; i++) put(c, x + (int)i, y + (int)j);
}
void canvas_draw_frame(Canvas* c, int32_t x, int32_t y, size_t w, size_t h) {
    canvas_draw_line(c, x, y, x + (int)w - 1, y);
    canvas_draw_line(c, x, y + (int)h - 1, x + (int)w - 1, y + (int)h - 1);
    canvas_draw_line(c, x, y, x, y + (int)h - 1);
    canvas_draw_line(c, x + (int)w - 1, y, x + (int)w - 1, y + (int)h - 1);
}
void canvas_draw_rframe(Canvas* c, int32_t x, int32_t y, size_t w, size_t h, size_t r) {
    (void)r;
    canvas_draw_frame(c, x, y, w, h);
}
void canvas_draw_circle(Canvas* c, int32_t cx, int32_t cy, size_t r) {
    for(int a = 0; a < 64; a++) {
        float t = a * 6.2831853f / 64;
        put(c, cx + (int)lroundf(cosf(t) * r), cy + (int)lroundf(sinf(t) * r));
    }
}
/* Text is drawn as small block placeholders: enough to check layout. */
static int cw(Canvas* c) {
    return c->font == FontPrimary ? 7 : 5;
}
static int ch(Canvas* c) {
    return c->font == FontPrimary ? 8 : 7;
}
uint16_t canvas_string_width(Canvas* c, const char* s) {
    return (uint16_t)(strlen(s) * cw(c));
}
void canvas_draw_str(Canvas* c, int32_t x, int32_t y, const char* s) {
    for(; *s; s++, x += cw(c)) {
        if(*s == ' ') continue;
        canvas_draw_box(c, x, y - ch(c) + 1, cw(c) - 1, ch(c) - 1);
    }
}
void canvas_draw_str_aligned(Canvas* c, int32_t x, int32_t y, Align h, Align v, const char* s) {
    int w = canvas_string_width(c, s);
    if(h == AlignCenter) x -= w / 2;
    if(h == AlignRight) x -= w;
    if(v == AlignTop) y += ch(c);
    if(v == AlignCenter) y += ch(c) / 2;
    canvas_draw_str(c, x, y, s);
}

/* ---- helpers ---- */

static Canvas canvas;

static void dump(const Game* g, const char* name) {
    game_draw(g, &canvas);
    char path[256];
    snprintf(path, sizeof(path), "test/out/%s.pgm", name);
    FILE* f = fopen(path, "wb");
    if(!f) {
        printf("cannot write %s\n", path);
        exit(1);
    }
    fprintf(f, "P5 128 64 255\n");
    for(int y = 0; y < 64; y++)
        for(int x = 0; x < 128; x++) fputc(canvas.px[y][x] ? 0 : 255, f);
    fclose(f);
}

static void press(Game* g, InputKey k) {
    game_input(g, k, InputTypePress);
    game_input(g, k, InputTypeRelease);
    game_input(g, k, InputTypeShort);
}

/* simple autopilot: hold fire, steer under the nearest enemy, dodge bullets */
static void bot(Game* g) {
    float target = g->px;
    float best = 1e9f;
    for(int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy* e = &g->enemies[i];
        if(!e->active) continue;
        float d = e->y > 0 ? (SCREEN_H - e->y) : 999;
        if(d < best) best = d, target = e->x;
    }
    for(int i = 0; i < MAX_EBULLETS; i++) {
        const Bullet* b = &g->ebullets[i];
        if(!b->active) continue;
        if(b->y > g->py - 14 && b->y < g->py + 4 && fabsf(b->x - g->px) < 6) {
            target = b->x < g->px ? g->px + 12 : g->px - 12;
        }
    }
    for(int i = 0; i < MAX_POWERUPS; i++)
        if(g->powerups[i].active && g->powerups[i].y > 30) target = g->powerups[i].x;
    g->held_ok = true;
    g->held_left = target < g->px - 1;
    g->held_right = target > g->px + 1;
    g->held_down = true;
}

static int fails = 0;
#define CHECK(cond, ...)                                  \
    do {                                                  \
        if(!(cond)) {                                     \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
            fails++;                                      \
        }                                                 \
    } while(0)

static void check_invariants(const Game* g) {
    CHECK(g->px >= 4 && g->px <= SCREEN_W - 5, "px out of range %f", g->px);
    CHECK(g->lives >= 0 && g->lives <= MAX_LIVES, "lives %d", g->lives);
    for(int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy* e = &g->enemies[i];
        if(e->active) CHECK(e->hp > 0 && e->hp <= e->max_hp, "enemy hp %d/%d", e->hp, e->max_hp);
    }
}

int main(void) {
    static Game game;
    Game* g = &game;

    /* --- 1. title & start --- */
    game_init(g, 42, 1234);
    CHECK(g->state == StateTitle, "starts on title");
    for(int i = 0; i < 20; i++) game_update(g);
    g->frame = 0;
    dump(g, "01_title");
    press(g, InputKeyOk);
    CHECK(g->state == StatePlaying, "OK starts game");
    CHECK(g->lives == 3 && g->wave == 1, "fresh game");
    game_update(g);
    dump(g, "02_wave_banner");

    /* --- enemy reaching the bottom costs a life, even with a shield --- */
    {
        static Game saved;
        saved = *g;
        g->shield_timer = 300;
        Enemy* e = &g->enemies[0];
        memset(e, 0, sizeof(*e));
        e->active = true;
        e->type = EnemyDrifter;
        e->hp = e->max_hp = 1;
        e->base_x = 10; /* far from the ship */
        e->y = SCREEN_H - 0.1f;
        e->vy = 1;
        g->held_ok = false;
        game_update(g);
        CHECK(!e->active, "escaped enemy removed");
        CHECK(g->lives == 2, "escape costs a life (lives=%d)", g->lives);
        CHECK(g->shield_timer > 0, "shield does not block escape penalty");
        g->lives = 1;
        e->active = true;
        e->y = SCREEN_H - 0.1f;
        game_update(g);
        CHECK(g->state == StateGameOver, "escape on last life ends the game");
        *g = saved;
    }

    /* --- 2. invincible bot plays through boss waves --- */
    int boss_seen = 0, boss_killed = 0, max_wave = 0, shots_dumped = 0;
    bool boss_dumped = false, pause_done = false;
    for(int f = 0; f < 30 * 60 * 8 && g->wave <= 11; f++) {
        bot(g);
        g->invuln_timer = 2; /* cheat for the progression test */
        g->lives = 3;
        int alive_boss_before = 0;
        for(int i = 0; i < MAX_ENEMIES; i++)
            if(g->enemies[i].active && g->enemies[i].type == EnemyBoss) alive_boss_before = 1;
        game_update(g);
        check_invariants(g);
        if(g->sfx & SFX_BOSS) boss_seen++;
        if(g->sfx & SFX_BOSS_KILL) boss_killed++;
        g->sfx = 0;
        if(g->wave > max_wave) max_wave = g->wave;
        (void)alive_boss_before;

        if(g->wave == 2 && shots_dumped == 0) {
            int n = 0;
            for(int i = 0; i < MAX_ENEMIES; i++) n += g->enemies[i].active;
            if(n >= 4) {
                g->invuln_timer = 0;
                dump(g, "03_gameplay");
                shots_dumped = 1;
            }
        }
        if(g->wave == 3 && !pause_done && g->banner_timer == 0) {
            g->double_timer = 300;
            g->shield_timer = 300;
            g->invuln_timer = 0;
            dump(g, "04_powerups");
            game_input(g, InputKeyBack, InputTypeShort);
            CHECK(g->state == StatePaused, "Back pauses");
            dump(g, "05_paused");
            game_update(g);
            CHECK(g->state == StatePaused, "paused game does not advance");
            press(g, InputKeyOk);
            CHECK(g->state == StatePlaying, "OK resumes");
            pause_done = true;
        }
        if(!boss_dumped && g->wave == 5) {
            for(int i = 0; i < MAX_ENEMIES; i++) {
                const Enemy* e = &g->enemies[i];
                if(e->active && e->type == EnemyBoss && e->hp < e->max_hp * 2 / 3) {
                    g->invuln_timer = 0;
                    dump(g, "06_boss");
                    boss_dumped = true;
                }
            }
        }
    }
    printf("progression: reached wave %d, bosses spawned %d, killed %d, score %lu\n", max_wave,
           boss_seen, boss_killed, (unsigned long)g->score);
    CHECK(max_wave >= 11, "should reach wave 11 (got %d)", max_wave);
    CHECK(boss_seen >= 2 && boss_killed >= 2, "two bosses spawned and killed");

    /* --- 3. honest bot until game over --- */
    int runs = 5;
    unsigned long total = 0;
    int best_wave = 0;
    for(int r = 0; r < runs; r++) {
        game_init(g, 1000 + r, 0);
        game_start(g);
        int f = 0;
        while(g->state == StatePlaying && f < 30 * 60 * 15) {
            bot(g);
            game_update(g);
            check_invariants(g);
            g->sfx = 0;
            f++;
        }
        CHECK(g->state == StateGameOver, "honest bot eventually dies (run %d)", r);
        CHECK(g->save_pending && g->high_score == g->score, "high score recorded");
        printf("run %d: wave %d, score %lu, %.1fs\n", r, g->wave, (unsigned long)g->score, f / 30.0);
        total += g->score;
        if(g->wave > best_wave) best_wave = g->wave;
    }
    printf("avg score %lu, best wave %d\n", total / runs, best_wave);
    for(int i = 0; i < 30; i++) game_update(g);
    dump(g, "07_game_over");

    press(g, InputKeyOk);
    CHECK(g->state == StatePlaying && g->lives == 3 && g->score == 0, "retry resets");
    CHECK(g->high_score > 0, "retry keeps high score");
    game_input(g, InputKeyBack, InputTypeShort);
    press(g, InputKeyBack);
    CHECK(g->state == StateTitle, "pause -> back goes to title");
    CHECK(game_input(g, InputKeyBack, InputTypeShort) == false, "back on title exits");
    game_start(g);
    CHECK(game_input(g, InputKeyBack, InputTypeLong) == false, "long back exits anywhere");

    printf(fails ? "%d FAILURES\n" : "ALL CHECKS PASSED\n", fails);
    return fails ? 1 : 0;
}
