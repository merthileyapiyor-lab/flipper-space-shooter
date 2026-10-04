#include "../common.h"

#define SM_MAX 64

typedef enum { SM_Show, SM_Input, SM_Wait, SM_Fail } SmPhase;

typedef struct {
    uint8_t seq[SM_MAX];
    int len, pos; /* pos: showing or input index */
    SmPhase phase;
    uint32_t t0; /* phase timer (ms) */
    int lit; /* pad currently lit, -1 none */
    bool started;
} Simon;

static const char* const sm_icon[] = {".#.#.", "#####", ".#.#.", "#####", ".#.#."};
static const Sprite spr_sm_icon = {5, 5, sm_icon};

/* four pads: Up, Right, Down, Left mapped to 0..3 */
static const InputKey sm_keys[4] = {InputKeyUp, InputKeyRight, InputKeyDown, InputKeyLeft};

static void sm_next(Simon* g, GameCtx* ctx) {
    if(g->len < SM_MAX) g->seq[g->len++] = rndi(ctx, 4);
    g->phase = SM_Show;
    g->pos = 0;
    g->t0 = ctx->now_ms;
    g->lit = -1;
}

static void sm_init(void* s, GameCtx* ctx) {
    (void)ctx;
    Simon* g = s;
    g->lit = -1;
    g->phase = SM_Wait;
}

static void sm_update(void* s, GameCtx* ctx) {
    Simon* g = s;
    if(!g->started) return;
    uint32_t dt = ctx->now_ms - g->t0;
    uint32_t step = (uint32_t)clampi(500 - g->len * 15, 220, 500);
    if(g->phase == SM_Show) {
        uint32_t on = step * 6 / 10;
        if(g->lit < 0 && dt < on) {
            g->lit = g->seq[g->pos];
            ctx->tone = g->lit + 1;
        } else if(g->lit >= 0 && dt >= on) {
            g->lit = -1;
        }
        if(dt >= step) {
            g->pos++;
            g->t0 = ctx->now_ms;
            g->lit = -1;
            if(g->pos >= g->len) {
                g->phase = SM_Input;
                g->pos = 0;
            }
        }
    } else if(g->phase == SM_Input) {
        if(g->lit >= 0 && dt > 160) g->lit = -1;
    }
}

static void sm_draw(void* s, Canvas* c, GameCtx* ctx) {
    (void)ctx;
    Simon* g = s;
    canvas_draw_str(c, 2, 8, "Simon");
    char buf[20];
    snprintf(buf, sizeof(buf), "Lvl %d", g->len);
    canvas_draw_str_aligned(c, SCREEN_W - 2, 8, AlignRight, AlignBottom, buf);

    int cx = SCREEN_W / 2, cy = 37, r = 11, gap = 14;
    /* pad positions: 0 up,1 right,2 down,3 left */
    int pxs[4] = {cx, cx + gap, cx, cx - gap};
    int pys[4] = {cy - gap, cy, cy + gap, cy};
    for(int i = 0; i < 4; i++) {
        bool on = (g->lit == i);
        if(on)
            canvas_draw_disc(c, pxs[i], pys[i], r);
        else {
            canvas_draw_circle(c, pxs[i], pys[i], r);
            canvas_draw_circle(c, pxs[i], pys[i], r - 3);
        }
    }
    if(!g->started)
        draw_str_boxed(c, SCREEN_W / 2, 60, AlignCenter, "OK start - watch pads");
    else if(g->phase == SM_Show)
        canvas_draw_str_aligned(c, SCREEN_W / 2, 62, AlignCenter, AlignBottom, "Watch...");
    else if(g->phase == SM_Input)
        canvas_draw_str_aligned(c, SCREEN_W / 2, 62, AlignCenter, AlignBottom, "Repeat!");
}

static void sm_input(void* s, GameCtx* ctx, InputKey key, InputType type) {
    Simon* g = s;
    if(type != InputTypePress) return;
    if(!g->started) {
        if(key == InputKeyOk) {
            g->started = true;
            sm_next(g, ctx);
        }
        return;
    }
    if(g->phase != SM_Input) return;
    int pad = -1;
    for(int i = 0; i < 4; i++)
        if(sm_keys[i] == key) pad = i;
    if(pad < 0) return;
    g->lit = pad;
    g->t0 = ctx->now_ms;
    ctx->tone = pad + 1;
    if(g->seq[g->pos] == pad) {
        g->pos++;
        if(g->pos >= g->len) {
            ctx->score = g->len;
            ctx->sfx |= SFX_POWER;
            sm_next(g, ctx);
        }
    } else {
        ctx->score = g->len - 1;
        ctx->over = true;
        ctx->sfx |= SFX_GAME_OVER;
    }
}

const GameDef game_simon = {
    .name = "Simon",
    .icon = &spr_sm_icon,
    .state_size = sizeof(Simon),
    .init = sm_init,
    .update = sm_update,
    .draw = sm_draw,
    .input = sm_input,
};
