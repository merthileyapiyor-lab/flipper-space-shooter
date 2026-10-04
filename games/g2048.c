#include "../common.h"

#define G_N 4
#define G_CELL 13
#define G_OX 2
#define G_OY 11

typedef struct {
    uint16_t v[G_N][G_N];
    bool won, lost, reached;
} G2048;

static const char* const g_icon[] = {"#.##.", "#.#.#", "###.#", "..#.#", "#.##."};
static const Sprite spr_g_icon = {5, 5, g_icon};

static void g_add(G2048* g, GameCtx* ctx) {
    int empty = 0;
    for(int y = 0; y < G_N; y++)
        for(int x = 0; x < G_N; x++)
            if(!g->v[y][x]) empty++;
    if(!empty) return;
    int k = rndi(ctx, empty);
    for(int y = 0; y < G_N; y++)
        for(int x = 0; x < G_N; x++)
            if(!g->v[y][x] && k-- == 0) g->v[y][x] = rndi(ctx, 10) ? 2 : 4;
}

static void g_init(void* s, GameCtx* ctx) {
    G2048* g = s;
    g_add(g, ctx);
    g_add(g, ctx);
}

/* slide+merge one line of 4 toward index 0; returns moved */
static bool g_line(uint16_t* a, GameCtx* ctx, int32_t* score) {
    uint16_t t[G_N] = {0};
    int n = 0;
    bool moved = false;
    for(int i = 0; i < G_N; i++)
        if(a[i]) t[n++] = a[i];
    uint16_t out[G_N] = {0};
    int o = 0;
    for(int i = 0; i < n; i++) {
        if(i + 1 < n && t[i] == t[i + 1]) {
            out[o] = t[i] * 2;
            *score += out[o];
            ctx->sfx |= SFX_BLIP;
            o++;
            i++;
        } else
            out[o++] = t[i];
    }
    for(int i = 0; i < G_N; i++) {
        if(a[i] != out[i]) moved = true;
        a[i] = out[i];
    }
    return moved;
}

static bool g_move(G2048* g, GameCtx* ctx, int dir) {
    /* dir 0 left 1 right 2 up 3 down */
    bool moved = false;
    for(int i = 0; i < G_N; i++) {
        uint16_t line[G_N];
        for(int j = 0; j < G_N; j++) {
            int x, y;
            if(dir == 0) x = j, y = i;
            else if(dir == 1) x = G_N - 1 - j, y = i;
            else if(dir == 2) x = i, y = j;
            else x = i, y = G_N - 1 - j;
            line[j] = g->v[y][x];
        }
        if(g_line(line, ctx, &ctx->score)) moved = true;
        for(int j = 0; j < G_N; j++) {
            int x, y;
            if(dir == 0) x = j, y = i;
            else if(dir == 1) x = G_N - 1 - j, y = i;
            else if(dir == 2) x = i, y = j;
            else x = i, y = G_N - 1 - j;
            g->v[y][x] = line[j];
        }
    }
    return moved;
}

static bool g_canmove(G2048* g) {
    for(int y = 0; y < G_N; y++)
        for(int x = 0; x < G_N; x++) {
            if(!g->v[y][x]) return true;
            if(x + 1 < G_N && g->v[y][x] == g->v[y][x + 1]) return true;
            if(y + 1 < G_N && g->v[y][x] == g->v[y + 1][x]) return true;
        }
    return false;
}

static void g_update(void* s, GameCtx* ctx) {
    (void)s;
    (void)ctx;
}

static void g_draw(void* s, Canvas* c, GameCtx* ctx) {
    G2048* g = s;
    canvas_draw_box(c, 0, 0, SCREEN_W, 9);
    canvas_set_color(c, ColorWhite);
    char buf[16];
    snprintf(buf, sizeof(buf), "2048  %ld", (long)ctx->score);
    canvas_draw_str(c, 2, 7, buf);
    canvas_set_color(c, ColorBlack);
    for(int y = 0; y < G_N; y++)
        for(int x = 0; x < G_N; x++) {
            int px = G_OX + x * (G_CELL + 1), py = G_OY + y * (G_CELL + 1);
            canvas_draw_rframe(c, px, py, G_CELL, G_CELL, 1);
            int v = g->v[y][x];
            if(v) {
                if(v >= 128) canvas_draw_box(c, px + 1, py + 1, G_CELL - 2, G_CELL - 2);
                canvas_set_color(c, v >= 128 ? ColorWhite : ColorBlack);
                int digits = v >= 1000 ? 4 : v >= 100 ? 3 : v >= 10 ? 2 : 1;
                draw_tiny_number(c, px + (G_CELL - digits * 4) / 2 + 1, py + 4, v);
                canvas_set_color(c, ColorBlack);
            }
        }
    /* side hint */
    canvas_draw_str(c, G_OX + G_N * (G_CELL + 1) + 4, 24, "Slide");
    canvas_draw_str(c, G_OX + G_N * (G_CELL + 1) + 4, 36, "tiles");
    canvas_draw_str(c, G_OX + G_N * (G_CELL + 1) + 4, 48, "to 2048");
}

static void g_finish_check(G2048* g, GameCtx* ctx) {
    if(!g->reached)
        for(int y = 0; y < G_N; y++)
            for(int x = 0; x < G_N; x++)
                if(g->v[y][x] >= 2048) {
                    g->reached = true;
                    ctx->won = true;
                    ctx->over = true;
                    ctx->sfx |= SFX_WIN;
                }
    if(!g_canmove(g)) {
        ctx->over = true;
        ctx->sfx |= SFX_GAME_OVER;
    }
}

static void g_input(void* s, GameCtx* ctx, InputKey key, InputType type) {
    G2048* g = s;
    if(type != InputTypePress && type != InputTypeRepeat) return;
    int dir = key == InputKeyLeft ? 0 : key == InputKeyRight ? 1 : key == InputKeyUp ? 2 : key == InputKeyDown ? 3 : -1;
    if(dir < 0) return;
    if(g_move(g, ctx, dir)) {
        g_add(g, ctx);
        g_finish_check(g, ctx);
    }
}

const GameDef game_2048 = {
    .name = "2048",
    .icon = &spr_g_icon,
    .state_size = sizeof(G2048),
    .init = g_init,
    .update = g_update,
    .draw = g_draw,
    .input = g_input,
};
