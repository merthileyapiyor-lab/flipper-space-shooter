#include "../common.h"

#define MN_W 12
#define MN_H 8
#define MN_CELL 7
#define MN_OX 2
#define MN_OY 10
#define MN_BOMBS 14

typedef struct {
    uint8_t mine[MN_H][MN_W];
    uint8_t open[MN_H][MN_W];
    uint8_t flag[MN_H][MN_W];
    int cx, cy, opened, flags;
    bool placed, dead;
    uint32_t start_ms;
} Mines;

static const char* const mn_icon[] = {".....", "..#..", ".###.", "#####", "..#.."};
static const Sprite spr_mn_icon = {5, 5, mn_icon};

static int mn_count(Mines* g, int x, int y) {
    int n = 0;
    for(int dy = -1; dy <= 1; dy++)
        for(int dx = -1; dx <= 1; dx++) {
            int nx = x + dx, ny = y + dy;
            if(nx >= 0 && nx < MN_W && ny >= 0 && ny < MN_H && g->mine[ny][nx]) n++;
        }
    return n;
}

static void mn_place(Mines* g, GameCtx* ctx, int safex, int safey) {
    int placed = 0;
    while(placed < MN_BOMBS) {
        int x = rndi(ctx, MN_W), y = rndi(ctx, MN_H);
        if(g->mine[y][x]) continue;
        if(x >= safex - 1 && x <= safex + 1 && y >= safey - 1 && y <= safey + 1) continue;
        g->mine[y][x] = 1;
        placed++;
    }
    g->placed = true;
    g->start_ms = ctx->now_ms;
}

static void mn_init(void* s, GameCtx* ctx) {
    (void)ctx;
    Mines* g = s;
    g->cx = MN_W / 2;
    g->cy = MN_H / 2;
}

static void mn_flood(Mines* g, int x, int y) {
    if(x < 0 || x >= MN_W || y < 0 || y >= MN_H || g->open[y][x] || g->flag[y][x]) return;
    g->open[y][x] = 1;
    g->opened++;
    if(mn_count(g, x, y) != 0) return;
    for(int dy = -1; dy <= 1; dy++)
        for(int dx = -1; dx <= 1; dx++)
            if(dx || dy) mn_flood(g, x + dx, y + dy);
}

static void mn_update(void* s, GameCtx* ctx) {
    Mines* g = s;
    if(g->placed && !ctx->over) ctx->score = (ctx->now_ms - g->start_ms) / 1000;
}

static void mn_reveal(Mines* g, GameCtx* ctx) {
    int x = g->cx, y = g->cy;
    if(g->open[y][x] || g->flag[y][x]) return;
    if(!g->placed) mn_place(g, ctx, x, y);
    if(g->mine[y][x]) {
        g->dead = true;
        ctx->over = true;
        ctx->sfx |= SFX_GAME_OVER;
        for(int yy = 0; yy < MN_H; yy++)
            for(int xx = 0; xx < MN_W; xx++)
                if(g->mine[yy][xx]) g->open[yy][xx] = 1;
        return;
    }
    mn_flood(g, x, y);
    ctx->sfx |= SFX_BLIP;
    if(g->opened == MN_W * MN_H - MN_BOMBS) {
        ctx->over = true;
        ctx->won = true;
        ctx->sfx |= SFX_WIN;
    }
}

static void mn_draw(void* s, Canvas* c, GameCtx* ctx) {
    Mines* g = s;
    canvas_draw_box(c, 0, 0, SCREEN_W, 9);
    canvas_set_color(c, ColorWhite);
    char buf[24];
    snprintf(buf, sizeof(buf), "Mines %d  %lds", MN_BOMBS - g->flags, (long)ctx->score);
    canvas_draw_str(c, 2, 7, buf);
    canvas_set_color(c, ColorBlack);
    for(int y = 0; y < MN_H; y++)
        for(int x = 0; x < MN_W; x++) {
            int px = MN_OX + x * MN_CELL, py = MN_OY + y * MN_CELL;
            if(g->open[y][x]) {
                if(g->mine[y][x]) {
                    canvas_draw_box(c, px + 1, py + 1, MN_CELL - 2, MN_CELL - 2);
                } else {
                    int n = mn_count(g, x, y);
                    if(n) draw_tiny_number(c, px + 2, py + 1, n);
                }
            } else {
                canvas_draw_rframe(c, px, py, MN_CELL, MN_CELL, 1);
                if(g->flag[y][x]) {
                    canvas_draw_line(c, px + 3, py + 1, px + 3, py + 5);
                    canvas_draw_box(c, px + 4, py + 1, 2, 2);
                }
            }
        }
    /* cursor */
    int cx = MN_OX + g->cx * MN_CELL, cy = MN_OY + g->cy * MN_CELL;
    if((ctx->now_ms / 150) % 2 || g->dead) canvas_draw_frame(c, cx, cy, MN_CELL, MN_CELL);
}

static void mn_input(void* s, GameCtx* ctx, InputKey key, InputType type) {
    Mines* g = s;
    if((type == InputTypePress || type == InputTypeRepeat)) {
        if(key == InputKeyLeft) g->cx = (g->cx + MN_W - 1) % MN_W;
        if(key == InputKeyRight) g->cx = (g->cx + 1) % MN_W;
        if(key == InputKeyUp) g->cy = (g->cy + MN_H - 1) % MN_H;
        if(key == InputKeyDown) g->cy = (g->cy + 1) % MN_H;
    }
    if(key == InputKeyOk && type == InputTypeShort) mn_reveal(g, ctx);
    if(key == InputKeyOk && type == InputTypeLong) {
        /* long press toggles a flag on an unopened cell */
        if(!g->open[g->cy][g->cx]) {
            if(g->flag[g->cy][g->cx]) {
                g->flag[g->cy][g->cx] = 0;
                g->flags--;
            } else {
                g->flag[g->cy][g->cx] = 1;
                g->flags++;
            }
            ctx->sfx |= SFX_TICK;
        }
    }
}

const GameDef game_mines = {
    .name = "Mines",
    .icon = &spr_mn_icon,
    .state_size = sizeof(Mines),
    .init = mn_init,
    .update = mn_update,
    .draw = mn_draw,
    .input = mn_input,
};
