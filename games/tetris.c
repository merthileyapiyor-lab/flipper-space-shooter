#include "../common.h"

#define TT_W 10
#define TT_H 16
#define TT_CELL 3
#define TT_OX 2
#define TT_OY 1

typedef struct {
    uint8_t grid[TT_H][TT_W];
    int piece, rot, px, py;
    int next;
    int tick, speed, lines;
    bool started;
} Tet;

static const char* const tt_icon[] = {"##...", "##.#.", "..###", ".....", "....."};
static const Sprite spr_tt_icon = {5, 5, tt_icon};

/* 7 tetromino shapes, 4 rotations, 4 blocks of (x,y) */
static const int8_t TET[7][4][4][2] = {
    {{{0,1},{1,1},{2,1},{3,1}},{{2,0},{2,1},{2,2},{2,3}},{{0,2},{1,2},{2,2},{3,2}},{{1,0},{1,1},{1,2},{1,3}}}, /* I */
    {{{0,0},{0,1},{1,1},{2,1}},{{1,0},{2,0},{1,1},{1,2}},{{0,1},{1,1},{2,1},{2,2}},{{1,0},{1,1},{0,2},{1,2}}}, /* J */
    {{{2,0},{0,1},{1,1},{2,1}},{{1,0},{1,1},{1,2},{2,2}},{{0,1},{1,1},{2,1},{0,2}},{{0,0},{1,0},{1,1},{1,2}}}, /* L */
    {{{1,0},{2,0},{1,1},{2,1}},{{1,0},{2,0},{1,1},{2,1}},{{1,0},{2,0},{1,1},{2,1}},{{1,0},{2,0},{1,1},{2,1}}}, /* O */
    {{{1,0},{2,0},{0,1},{1,1}},{{1,0},{1,1},{2,1},{2,2}},{{1,1},{2,1},{0,2},{1,2}},{{0,0},{0,1},{1,1},{1,2}}}, /* S */
    {{{1,0},{0,1},{1,1},{2,1}},{{1,0},{1,1},{2,1},{1,2}},{{0,1},{1,1},{2,1},{1,2}},{{1,0},{0,1},{1,1},{1,2}}}, /* T */
    {{{0,0},{1,0},{1,1},{2,1}},{{2,0},{1,1},{2,1},{1,2}},{{0,1},{1,1},{1,2},{2,2}},{{1,0},{0,1},{1,1},{0,2}}}, /* Z */
};

static bool tt_fits(Tet* g, int piece, int rot, int px, int py) {
    for(int i = 0; i < 4; i++) {
        int x = px + TET[piece][rot][i][0], y = py + TET[piece][rot][i][1];
        if(x < 0 || x >= TT_W || y >= TT_H) return false;
        if(y >= 0 && g->grid[y][x]) return false;
    }
    return true;
}

static void tt_spawn(Tet* g, GameCtx* ctx) {
    g->piece = g->next;
    g->next = rndi(ctx, 7);
    g->rot = 0;
    g->px = 3;
    g->py = -1;
    if(!tt_fits(g, g->piece, 0, g->px, g->py)) {
        ctx->over = true;
        ctx->sfx |= SFX_GAME_OVER;
    }
}

static void tt_init(void* s, GameCtx* ctx) {
    Tet* g = s;
    g->speed = 18;
    g->next = rndi(ctx, 7);
    tt_spawn(g, ctx);
}

static void tt_lock(Tet* g, GameCtx* ctx) {
    for(int i = 0; i < 4; i++) {
        int x = g->px + TET[g->piece][g->rot][i][0], y = g->py + TET[g->piece][g->rot][i][1];
        if(y >= 0) g->grid[y][x] = 1;
    }
    int cleared = 0;
    for(int y = TT_H - 1; y >= 0; y--) {
        bool full = true;
        for(int x = 0; x < TT_W; x++)
            if(!g->grid[y][x]) full = false;
        if(full) {
            cleared++;
            for(int yy = y; yy > 0; yy--)
                for(int x = 0; x < TT_W; x++) g->grid[yy][x] = g->grid[yy - 1][x];
            for(int x = 0; x < TT_W; x++) g->grid[0][x] = 0;
            y++;
        }
    }
    if(cleared) {
        static const int pts[] = {0, 10, 25, 45, 70};
        ctx->score += pts[cleared];
        g->lines += cleared;
        ctx->sfx |= SFX_POWER;
        g->speed = clampi(18 - g->lines / 5, 4, 18);
    } else {
        ctx->sfx |= SFX_BLIP;
    }
    tt_spawn(g, ctx);
}

static void tt_update(void* s, GameCtx* ctx) {
    Tet* g = s;
    if(!g->started) return;
    int sp = (ctx->held & KEY_BIT(InputKeyDown)) ? 2 : g->speed;
    if(++g->tick < sp) return;
    g->tick = 0;
    if(tt_fits(g, g->piece, g->rot, g->px, g->py + 1))
        g->py++;
    else
        tt_lock(g, ctx);
}

static void tt_draw(void* s, Canvas* c, GameCtx* ctx) {
    Tet* g = s;
    int fw = TT_W * TT_CELL, fh = TT_H * TT_CELL;
    canvas_draw_frame(c, TT_OX - 1, TT_OY - 1, fw + 2, fh + 2);
    for(int y = 0; y < TT_H; y++)
        for(int x = 0; x < TT_W; x++)
            if(g->grid[y][x])
                canvas_draw_box(c, TT_OX + x * TT_CELL, TT_OY + y * TT_CELL, TT_CELL - 1, TT_CELL - 1);
    for(int i = 0; i < 4; i++) {
        int x = g->px + TET[g->piece][g->rot][i][0], y = g->py + TET[g->piece][g->rot][i][1];
        if(y >= 0) canvas_draw_box(c, TT_OX + x * TT_CELL, TT_OY + y * TT_CELL, TT_CELL - 1, TT_CELL - 1);
    }
    int ix = fw + 10;
    canvas_draw_str(c, ix, 8, "NEXT");
    for(int i = 0; i < 4; i++) {
        int x = TET[g->next][0][i][0], y = TET[g->next][0][i][1];
        canvas_draw_box(c, ix + x * 3, 12 + y * 3, 2, 2);
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", (long)ctx->score);
    canvas_draw_str(c, ix, 34, "SCORE");
    canvas_draw_str(c, ix, 44, buf);
    snprintf(buf, sizeof(buf), "Ln %d", g->lines);
    canvas_draw_str(c, ix, 58, buf);
    if(!g->started) draw_str_boxed(c, SCREEN_W / 2, 32, AlignCenter, "OK rotate");
}

static void tt_input(void* s, GameCtx* ctx, InputKey key, InputType type) {
    Tet* g = s;
    if(type != InputTypePress && type != InputTypeRepeat) return;
    g->started = true;
    if(key == InputKeyLeft && tt_fits(g, g->piece, g->rot, g->px - 1, g->py)) g->px--;
    if(key == InputKeyRight && tt_fits(g, g->piece, g->rot, g->px + 1, g->py)) g->px++;
    if(key == InputKeyUp) {
        int nr = (g->rot + 1) % 4;
        if(tt_fits(g, g->piece, nr, g->px, g->py)) g->rot = nr;
        else if(tt_fits(g, g->piece, nr, g->px - 1, g->py)) g->rot = nr, g->px--;
        else if(tt_fits(g, g->piece, nr, g->px + 1, g->py)) g->rot = nr, g->px++;
    }
    if(key == InputKeyOk) {
        while(tt_fits(g, g->piece, g->rot, g->px, g->py + 1)) g->py++;
        tt_lock(g, ctx);
    }
}

const GameDef game_tetris = {
    .name = "Tetris",
    .icon = &spr_tt_icon,
    .state_size = sizeof(Tet),
    .init = tt_init,
    .update = tt_update,
    .draw = tt_draw,
    .input = tt_input,
};
