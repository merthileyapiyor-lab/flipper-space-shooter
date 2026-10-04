/* PC test harness for Game Station: exercises every game with a random bot,
   checks invariants, and dumps one frame per game as PGM. */
#include "../station.h"

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
void canvas_draw_rbox(Canvas* c, int32_t x, int32_t y, size_t w, size_t h, size_t r) {
    (void)r;
    canvas_draw_box(c, x, y, w, h);
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
    for(int a = 0; a < 48; a++) {
        float t = a * 6.2831853f / 48;
        put(c, cx + (int)lroundf(cosf(t) * r), cy + (int)lroundf(sinf(t) * r));
    }
}
void canvas_draw_disc(Canvas* c, int32_t cx, int32_t cy, size_t r) {
    for(int yy = -(int)r; yy <= (int)r; yy++)
        for(int xx = -(int)r; xx <= (int)r; xx++)
            if(xx * xx + yy * yy <= (int)(r * r)) put(c, cx + xx, cy + yy);
}
static int cw(Canvas* c) {
    return c->font == FontPrimary ? 7 : 5;
}
static int chh(Canvas* c) {
    return c->font == FontPrimary ? 8 : 7;
}
uint16_t canvas_string_width(Canvas* c, const char* s) {
    return (uint16_t)(strlen(s) * cw(c));
}
void canvas_draw_str(Canvas* c, int32_t x, int32_t y, const char* s) {
    for(; *s; s++, x += cw(c)) {
        if(*s == ' ') continue;
        canvas_draw_box(c, x, y - chh(c) + 1, cw(c) - 1, chh(c) - 1);
    }
}
void canvas_draw_str_aligned(Canvas* c, int32_t x, int32_t y, Align h, Align v, const char* s) {
    int w = canvas_string_width(c, s);
    if(h == AlignCenter) x -= w / 2;
    if(h == AlignRight) x -= w;
    if(v == AlignTop) y += chh(c);
    if(v == AlignCenter) y += chh(c) / 2;
    canvas_draw_str(c, x, y, s);
}

/* ---- harness ---- */

static Canvas canvas;
static int fails = 0;
#define CHECK(cond, ...)                                 \
    do {                                                 \
        if(!(cond)) {                                    \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);  \
            printf(__VA_ARGS__);                         \
            printf("\n");                                \
            fails++;                                     \
        }                                                \
    } while(0)

static void dump(Station* st, const char* name) {
    station_draw(st, &canvas);
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

static uint32_t clk = 0; /* fake ms clock, advances 33ms per frame */

static void feed(Station* st, InputKey k, InputType t) {
    station_input(st, k, t, clk);
}
static void tap(Station* st, InputKey k) {
    feed(st, k, InputTypePress);
    feed(st, k, InputTypeRelease);
    feed(st, k, InputTypeShort);
}

static const InputKey allkeys[5] = {InputKeyUp, InputKeyDown, InputKeyLeft, InputKeyRight, InputKeyOk};

/* random bot: occasionally press/release keys, advance time, tick */
static void bot_play(Station* st, int frames, uint32_t* seed) {
    uint8_t held = 0;
    for(int f = 0; f < frames; f++) {
        *seed = *seed * 1103515245u + 12345u;
        int r = (*seed >> 16) & 0xFF;
        if(r < 90) {
            InputKey k = allkeys[r % 5];
            uint8_t bit = 1 << k;
            if(held & bit) {
                feed(st, k, InputTypeRelease);
                held &= ~bit;
            } else {
                feed(st, k, InputTypePress);
                held |= bit;
                if((r & 1)) feed(st, k, InputTypeShort);
            }
        }
        /* OK long press occasionally (mines flag) */
        if(r == 200) feed(st, InputKeyOk, InputTypeLong);
        clk += 33;
        station_tick(st, clk);
        /* validate shared ctx stays sane */
        CHECK(st->sel >= 0 && st->sel < GAME_COUNT, "sel range %d", st->sel);
    }
    /* release everything */
    for(int i = 0; i < 5; i++) feed(st, allkeys[i], InputTypeRelease);
}

int main(void) {
    static Station st;
    uint32_t best0[GAME_COUNT] = {0};
    station_init(&st, 12345, best0);

    CHECK(st.state == StationMenu, "starts in menu");
    clk = 100;
    for(int i = 0; i < 5; i++) station_tick(&st, clk += 33);
    dump(&st, "00_menu");

    /* scroll through the whole menu */
    for(int i = 0; i < GAME_COUNT + 2; i++) feed(&st, InputKeyDown, InputTypePress);
    CHECK(st.state == StationMenu, "menu survives scrolling");
    dump(&st, "00_menu_scrolled");

    const char* names[GAME_COUNT];
    for(int gi = 0; gi < GAME_COUNT; gi++) names[gi] = station_games[gi]->name;

    uint32_t seed = 99;
    for(int gi = 0; gi < GAME_COUNT; gi++) {
        /* go to menu, select game gi */
        if(st.state != StationMenu) {
            feed(&st, InputKeyBack, InputTypeShort); /* pause or over -> menu */
            if(st.state != StationMenu) feed(&st, InputKeyBack, InputTypeShort);
        }
        st.sel = gi;
        st.scroll = gi > GAME_COUNT - 4 ? GAME_COUNT - 4 : gi;
        tap(&st, InputKeyOk);
        CHECK(st.state == StationPlaying, "%s: OK starts game", names[gi]);
        CHECK(st.game == station_games[gi], "%s: correct game running", names[gi]);

        /* let it start, grab a mid-game frame */
        for(int i = 0; i < 20; i++) {
            clk += 33;
            station_tick(&st, clk);
        }
        /* nudge a few inputs so the frame looks alive */
        tap(&st, InputKeyOk);
        feed(&st, InputKeyRight, InputTypePress);
        for(int i = 0; i < 40; i++) {
            clk += 33;
            station_tick(&st, clk);
        }
        feed(&st, InputKeyRight, InputTypeRelease);
        char fname[32];
        snprintf(fname, sizeof(fname), "%02d_%s", gi + 1, names[gi]);
        for(char* p = fname; *p; p++)
            if(*p == ' ') *p = '_';
        if(st.state == StationPlaying) dump(&st, fname);

        /* now hammer it with a random bot; it must stay valid and not hang */
        bot_play(&st, 4000, &seed);

        /* pause/resume cycle works whenever still playing */
        if(st.state == StationPlaying) {
            feed(&st, InputKeyBack, InputTypeShort);
            CHECK(st.state == StationPaused, "%s: Back pauses", names[gi]);
            station_tick(&st, clk += 33);
            feed(&st, InputKeyOk, InputTypeShort);
            CHECK(st.state == StationPlaying, "%s: OK resumes", names[gi]);
        }
    }

    /* verify a game can actually end and record a best score (snake walls) */
    for(int tries = 0; tries < 3; tries++) {
        while(st.state != StationMenu) feed(&st, InputKeyBack, InputTypeShort);
        st.sel = 1; /* snake */
        st.scroll = 0;
        tap(&st, InputKeyOk);
        /* drive straight up into the wall */
        feed(&st, InputKeyUp, InputTypePress);
        feed(&st, InputKeyUp, InputTypeRelease);
        for(int i = 0; i < 120 && st.state == StationPlaying; i++) station_tick(&st, clk += 33);
        if(st.state == StationOver) break;
    }
    CHECK(st.state == StationOver, "snake can reach game over");
    dump(&st, "99_over");
    tap(&st, InputKeyOk);
    CHECK(st.state == StationPlaying, "OK retries from over screen");

    /* exit via long back */
    bool keep = station_input(&st, InputKeyBack, InputTypeLong, clk);
    CHECK(keep == false, "long Back exits app");

    station_free(&st);
    printf(fails ? "%d FAILURES\n" : "ALL CHECKS PASSED\n", fails);
    return fails ? 1 : 0;
}
