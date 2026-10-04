#include "../common.h"

#define SN_CELL 4
#define SN_COLS (SCREEN_W / SN_CELL)      /* 32 */
#define SN_ROWS ((SCREEN_H - 10) / SN_CELL) /* 13 */
#define SN_TOP 10
#define SN_MAX (SN_COLS * SN_ROWS)

typedef struct {
    uint8_t bx[SN_MAX], by[SN_MAX];
    int len, dx, dy, ndx, ndy;
    uint8_t fx, fy;
    int tick, speed;
    bool started;
} Snake;

static const char* const sn_icon[] = {"###..", "..#..", "..###", "....#", "..###"};
static const Sprite spr_sn_icon = {5, 5, sn_icon};

static void sn_food(Snake* g, GameCtx* ctx) {
    for(int tries = 0; tries < 200; tries++) {
        int x = rndi(ctx, SN_COLS), y = rndi(ctx, SN_ROWS);
        bool hit = false;
        for(int i = 0; i < g->len; i++)
            if(g->bx[i] == x && g->by[i] == y) hit = true;
        if(!hit) {
            g->fx = x;
            g->fy = y;
            return;
        }
    }
}

static void sn_init(void* s, GameCtx* ctx) {
    Snake* g = s;
    g->len = 3;
    for(int i = 0; i < 3; i++) g->bx[i] = 6 - i, g->by[i] = SN_ROWS / 2;
    g->dx = 1;
    g->dy = 0;
    g->ndx = 1;
    g->ndy = 0;
    g->speed = 7;
    sn_food(g, ctx);
}

static void sn_update(void* s, GameCtx* ctx) {
    Snake* g = s;
    if(!g->started) return;
    if(++g->tick < g->speed) return;
    g->tick = 0;
    g->dx = g->ndx;
    g->dy = g->ndy;
    int nx = g->bx[0] + g->dx, ny = g->by[0] + g->dy;
    if(nx < 0 || nx >= SN_COLS || ny < 0 || ny >= SN_ROWS) {
        ctx->over = true;
        ctx->sfx |= SFX_GAME_OVER;
        return;
    }
    bool grow = (nx == g->fx && ny == g->fy);
    int tail = grow ? g->len : g->len - 1;
    for(int i = 0; i < tail; i++)
        if(g->bx[i] == nx && g->by[i] == ny) {
            ctx->over = true;
            ctx->sfx |= SFX_GAME_OVER;
            return;
        }
    for(int i = g->len; i > 0; i--) g->bx[i] = g->bx[i - 1], g->by[i] = g->by[i - 1];
    g->bx[0] = nx;
    g->by[0] = ny;
    if(grow) {
        if(g->len < SN_MAX) g->len++;
        ctx->score += 1;
        ctx->sfx |= SFX_BLIP;
        if(g->speed > 2 && g->len % 5 == 0) g->speed--;
        if(g->len >= SN_MAX) {
            ctx->over = true;
            ctx->won = true;
            ctx->sfx |= SFX_WIN;
            return;
        }
        sn_food(g, ctx);
    }
}

static void sn_draw(void* s, Canvas* c, GameCtx* ctx) {
    Snake* g = s;
    canvas_draw_box(c, 0, 0, SCREEN_W, 9);
    canvas_set_color(c, ColorWhite);
    char buf[16];
    snprintf(buf, sizeof(buf), "Snake  %ld", (long)ctx->score);
    canvas_draw_str(c, 2, 7, buf);
    canvas_set_color(c, ColorBlack);
    canvas_draw_frame(c, 0, SN_TOP - 1, SN_COLS * SN_CELL + 1, SN_ROWS * SN_CELL + 1);
    int fx = g->fx * SN_CELL, fy = SN_TOP + g->fy * SN_CELL;
    canvas_draw_frame(c, fx, fy, SN_CELL, SN_CELL);
    canvas_draw_dot(c, fx + 1, fy + 1);
    for(int i = 0; i < g->len; i++) {
        int x = g->bx[i] * SN_CELL, y = SN_TOP + g->by[i] * SN_CELL;
        if(i == 0)
            canvas_draw_box(c, x, y, SN_CELL, SN_CELL);
        else
            canvas_draw_box(c, x + 1, y + 1, SN_CELL - 1, SN_CELL - 1);
    }
    if(!g->started) draw_str_boxed(c, SCREEN_W / 2, 38, AlignCenter, "Arrows to start");
}

static void sn_input(void* s, GameCtx* ctx, InputKey key, InputType type) {
    (void)ctx;
    Snake* g = s;
    if(type != InputTypePress && type != InputTypeRepeat) return;
    g->started = true;
    if(key == InputKeyUp && g->dy == 0) g->ndx = 0, g->ndy = -1;
    if(key == InputKeyDown && g->dy == 0) g->ndx = 0, g->ndy = 1;
    if(key == InputKeyLeft && g->dx == 0) g->ndx = -1, g->ndy = 0;
    if(key == InputKeyRight && g->dx == 0) g->ndx = 1, g->ndy = 0;
}

const GameDef game_snake = {
    .name = "Snake",
    .icon = &spr_sn_icon,
    .state_size = sizeof(Snake),
    .init = sn_init,
    .update = sn_update,
    .draw = sn_draw,
    .input = sn_input,
};
