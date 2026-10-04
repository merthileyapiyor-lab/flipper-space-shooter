#include "../common.h"

#define PG_PADH 14
#define PG_TOP 10
#define PG_WIN 5

typedef struct {
    float py, ay; /* player, ai paddle y (top) */
    float bx, by, vx, vy;
    int pscore, ascore;
    float aispeed;
    bool serving;
} Pong;

static const char* const pg_icon[] = {"#...#", "#.#.#", "#.#.#", "#.#.#", "#...#"};
static const Sprite spr_pg_icon = {5, 5, pg_icon};

static void pg_serve(Pong* g, GameCtx* ctx, int dir) {
    g->bx = SCREEN_W / 2;
    g->by = (PG_TOP + SCREEN_H) / 2;
    g->vx = dir * 1.8f;
    g->vy = (rndf(ctx) - 0.5f) * 2.0f;
    g->serving = true;
}

static void pg_init(void* s, GameCtx* ctx) {
    Pong* g = s;
    g->py = g->ay = (PG_TOP + SCREEN_H) / 2 - PG_PADH / 2;
    g->aispeed = 1.5f;
    pg_serve(g, ctx, 1);
}

static void pg_update(void* s, GameCtx* ctx) {
    Pong* g = s;
    float pv = 2.6f;
    if(ctx->held & KEY_BIT(InputKeyUp)) g->py -= pv;
    if(ctx->held & KEY_BIT(InputKeyDown)) g->py += pv;
    g->py = clampf(g->py, PG_TOP, SCREEN_H - PG_PADH);

    /* ai tracks the ball, imperfectly */
    float target = g->by - PG_PADH / 2;
    if(g->ay + PG_PADH / 2 < target - 2) g->ay += g->aispeed;
    else if(g->ay + PG_PADH / 2 > target + 2) g->ay -= g->aispeed;
    g->ay = clampf(g->ay, PG_TOP, SCREEN_H - PG_PADH);

    if(g->serving) {
        g->bx += g->vx;
        g->by += g->vy;
        if(g->by < PG_TOP + 1) g->by = PG_TOP + 1, g->vy = -g->vy;
        if(g->by > SCREEN_H - 1) g->by = SCREEN_H - 1, g->vy = -g->vy;
        if(g->vx < 0 && g->bx <= 4 && g->by >= g->py && g->by <= g->py + PG_PADH) {
            g->vx = -g->vx * 1.03f;
            g->vy += ((g->by - (g->py + PG_PADH / 2)) / (PG_PADH / 2)) * 1.3f;
            g->bx = 4;
            ctx->sfx |= SFX_BLIP;
        }
        if(g->vx > 0 && g->bx >= SCREEN_W - 4 && g->by >= g->ay && g->by <= g->ay + PG_PADH) {
            g->vx = -g->vx * 1.03f;
            g->vy += ((g->by - (g->ay + PG_PADH / 2)) / (PG_PADH / 2)) * 1.3f;
            g->bx = SCREEN_W - 4;
            ctx->sfx |= SFX_BLIP;
        }
        g->vy = clampf(g->vy, -3, 3);
        if(g->bx < 0) {
            g->ascore++;
            ctx->sfx |= SFX_HIT;
            g->serving = false;
        } else if(g->bx > SCREEN_W) {
            g->pscore++;
            ctx->score = g->pscore;
            ctx->sfx |= SFX_POWER;
            g->aispeed += 0.15f;
            g->serving = false;
        }
        if(!g->serving) {
            if(g->pscore >= PG_WIN) {
                ctx->over = true;
                ctx->won = true;
                ctx->sfx |= SFX_WIN;
            } else if(g->ascore >= PG_WIN) {
                ctx->over = true;
                ctx->sfx |= SFX_GAME_OVER;
            } else {
                pg_serve(g, ctx, g->bx < 0 ? 1 : -1);
            }
        }
    }
}

static void pg_draw(void* s, Canvas* c, GameCtx* ctx) {
    (void)ctx;
    Pong* g = s;
    canvas_draw_box(c, 0, 0, SCREEN_W, 9);
    canvas_set_color(c, ColorWhite);
    char buf[16];
    snprintf(buf, sizeof(buf), "You %d", g->pscore);
    canvas_draw_str(c, 2, 7, buf);
    snprintf(buf, sizeof(buf), "CPU %d", g->ascore);
    canvas_draw_str_aligned(c, SCREEN_W - 2, 7, AlignRight, AlignBottom, buf);
    canvas_set_color(c, ColorBlack);
    for(int y = PG_TOP; y < SCREEN_H; y += 5) canvas_draw_line(c, SCREEN_W / 2, y, SCREEN_W / 2, y + 2);
    canvas_draw_box(c, 1, (int)g->py, 3, PG_PADH);
    canvas_draw_box(c, SCREEN_W - 4, (int)g->ay, 3, PG_PADH);
    canvas_draw_box(c, (int)g->bx - 1, (int)g->by - 1, 2, 2);
}

static void pg_input(void* s, GameCtx* ctx, InputKey key, InputType type) {
    (void)s;
    (void)ctx;
    (void)key;
    (void)type;
}

const GameDef game_pong = {
    .name = "Pong",
    .icon = &spr_pg_icon,
    .state_size = sizeof(Pong),
    .init = pg_init,
    .update = pg_update,
    .draw = pg_draw,
    .input = pg_input,
};
