#include "common.h"

#include <string.h>

void draw_sprite(Canvas* c, int x, int y, const Sprite* s, int min_y) {
    for(int row = 0; row < s->h; row++) {
        int py = y + row;
        if(py < min_y || py < 0 || py >= SCREEN_H) continue;
        const char* line = s->rows[row];
        for(int col = 0; col < s->w; col++) {
            int px = x + col;
            if(px < 0 || px >= SCREEN_W) continue;
            if(line[col] == '#') canvas_draw_dot(c, px, py);
        }
    }
}

/* each digit: 5 rows of 3 bits, MSB = left */
static const uint8_t tiny_digits[10][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 3, 1, 7}, {5, 5, 7, 1, 1},
    {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 2, 2, 2}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
};

void draw_tiny_number(Canvas* c, int x, int y, int n) {
    char buf[12];
    int len = 0;
    if(n < 0) n = 0;
    do {
        buf[len++] = (char)(n % 10);
        n /= 10;
    } while(n && len < 11);
    for(int i = len - 1; i >= 0; i--) {
        const uint8_t* d = tiny_digits[(int)buf[i]];
        for(int r = 0; r < 5; r++)
            for(int b = 0; b < 3; b++)
                if(d[r] & (4 >> b)) canvas_draw_dot(c, x + b, y + r);
        x += 4;
    }
}

void draw_str_boxed(Canvas* c, int x, int y, Align h, const char* str) {
    int w = canvas_string_width(c, str) + 4;
    int bx = h == AlignCenter ? x - w / 2 : (h == AlignRight ? x - w : x - 2);
    canvas_set_color(c, ColorWhite);
    canvas_draw_box(c, bx, y - 9, w, 11);
    canvas_set_color(c, ColorBlack);
    canvas_draw_str_aligned(c, x, y, h, AlignBottom, str);
}

static const char* const heart_rows[] = {".#.#.", "#####", "#####", ".###.", "..#.."};
static const Sprite spr_heart = {5, 5, heart_rows};

void draw_hearts(Canvas* c, int right_x, int y, int count) {
    for(int i = 0; i < count; i++) draw_sprite(c, right_x - 5 - i * 6, y, &spr_heart, 0);
}

uint32_t rnd(GameCtx* ctx) {
    uint32_t x = ctx->rng ? ctx->rng : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    ctx->rng = x;
    return x;
}

float rndf(GameCtx* ctx) {
    return (float)(rnd(ctx) & 0xFFFF) / 65536.0f;
}

int rndi(GameCtx* ctx, int n) {
    return n <= 1 ? 0 : (int)(rnd(ctx) % (uint32_t)n);
}

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

bool overlap(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh) {
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}
