#include "../common.h"

/* 3x3 grid of holes; a mole pops up, hit it with the matching key before it hides.
   Holes are addressed with a cursor you move; OK whacks. 30 seconds. */

#define WH_TIME 30000

typedef struct {
    int cx, cy; /* cursor 0..2 */
    int mole; /* 0..8 active hole, -1 none */
    uint32_t mole_t0, mole_life;
    uint32_t start_ms;
    int misses;
    bool started, hit_flash;
    uint32_t flash_t0;
    int last_result; /* 0 none, 1 hit, 2 miss */
} Whack;

static const char* const wh_icon[] = {".....", ".###.", "#####", "#####", ".#.#."};
static const Sprite spr_wh_icon = {5, 5, wh_icon};

static void wh_pop(Whack* g, GameCtx* ctx) {
    g->mole = rndi(ctx, 9);
    g->mole_t0 = ctx->now_ms;
    int sec = (ctx->now_ms - g->start_ms) / 1000;
    g->mole_life = clampi(1100 - sec * 25, 450, 1100);
}

static void wh_init(void* s, GameCtx* ctx) {
    (void)ctx;
    Whack* g = s;
    g->cx = 1;
    g->cy = 1;
    g->mole = -1;
}

static void wh_update(void* s, GameCtx* ctx) {
    Whack* g = s;
    if(!g->started) return;
    uint32_t elapsed = ctx->now_ms - g->start_ms;
    if(elapsed >= WH_TIME) {
        ctx->over = true;
        ctx->won = true;
        ctx->sfx |= SFX_WIN;
        return;
    }
    if(g->hit_flash && ctx->now_ms - g->flash_t0 > 150) g->hit_flash = false;
    if(g->mole < 0) {
        wh_pop(g, ctx);
    } else if(ctx->now_ms - g->mole_t0 > g->mole_life) {
        g->mole = -1;
        g->misses++;
        g->last_result = 2;
        g->flash_t0 = ctx->now_ms;
        g->hit_flash = true;
    }
}

static void wh_draw(void* s, Canvas* c, GameCtx* ctx) {
    Whack* g = s;
    canvas_draw_box(c, 0, 0, SCREEN_W, 9);
    canvas_set_color(c, ColorWhite);
    char buf[24];
    int left = g->started ? clampi((WH_TIME - (int)(ctx->now_ms - g->start_ms)) / 1000, 0, 99) : 30;
    snprintf(buf, sizeof(buf), "Hits %ld  %02ds", (long)ctx->score, left);
    canvas_draw_str(c, 2, 7, buf);
    canvas_set_color(c, ColorBlack);

    int ox = 30, oy = 13, cell = 16;
    for(int i = 0; i < 9; i++) {
        int gx = i % 3, gy = i / 3;
        int px = ox + gx * cell, py = oy + gy * cell;
        canvas_draw_circle(c, px + 6, py + 8, 6); /* hole rim */
        if(g->mole == i) {
            canvas_draw_disc(c, px + 6, py + 6, 5);
            canvas_set_color(c, ColorWhite);
            canvas_draw_dot(c, px + 4, py + 5);
            canvas_draw_dot(c, px + 8, py + 5);
            canvas_set_color(c, ColorBlack);
        }
        if(g->cx == gx && g->cy == gy) canvas_draw_frame(c, px - 1, py - 1, 14, 16);
    }
    if(g->hit_flash)
        draw_str_boxed(c, SCREEN_W / 2, 62, AlignCenter, g->last_result == 1 ? "HIT!" : "missed");
    if(!g->started) draw_str_boxed(c, SCREEN_W / 2, 40, AlignCenter, "Move + OK to whack");
}

static void wh_input(void* s, GameCtx* ctx, InputKey key, InputType type) {
    Whack* g = s;
    if(type != InputTypePress && type != InputTypeRepeat) return;
    if(!g->started && key == InputKeyOk) {
        g->started = true;
        g->start_ms = ctx->now_ms;
        wh_pop(g, ctx);
        return;
    }
    if(key == InputKeyLeft) g->cx = (g->cx + 2) % 3;
    if(key == InputKeyRight) g->cx = (g->cx + 1) % 3;
    if(key == InputKeyUp) g->cy = (g->cy + 2) % 3;
    if(key == InputKeyDown) g->cy = (g->cy + 1) % 3;
    if(key == InputKeyOk && g->started) {
        int cell = g->cy * 3 + g->cx;
        if(g->mole == cell) {
            ctx->score += 1;
            ctx->sfx |= SFX_BLIP;
            g->mole = -1;
            g->last_result = 1;
            g->flash_t0 = ctx->now_ms;
            g->hit_flash = true;
        } else {
            ctx->sfx |= SFX_TICK;
        }
    }
}

const GameDef game_whack = {
    .name = "Whack Mole",
    .icon = &spr_wh_icon,
    .state_size = sizeof(Whack),
    .init = wh_init,
    .update = wh_update,
    .draw = wh_draw,
    .input = wh_input,
};
