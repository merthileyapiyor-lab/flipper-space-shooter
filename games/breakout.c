#include "../common.h"

#define BR_COLS 10
#define BR_ROWS 4
#define BR_TOP 12
#define BR_BW 12
#define BR_BH 4
#define BR_PADW 20

typedef struct {
    uint8_t brick[BR_ROWS][BR_COLS];
    int alive, level, lives;
    float bx, by, vx, vy, pad;
    bool launched;
} Brk;

static const char* const br_icon[] = {"#####", ".....", "..#..", ".###.", "....."};
static const Sprite spr_br_icon = {5, 5, br_icon};

static void br_level(Brk* g) {
    g->alive = 0;
    for(int r = 0; r < BR_ROWS; r++)
        for(int cc = 0; cc < BR_COLS; cc++) g->brick[r][cc] = 1, g->alive++;
    g->bx = SCREEN_W / 2;
    g->by = SCREEN_H - 10;
    g->vx = 0;
    g->vy = 0;
    g->pad = SCREEN_W / 2 - BR_PADW / 2;
    g->launched = false;
}

static void br_init(void* s, GameCtx* ctx) {
    (void)ctx;
    Brk* g = s;
    g->level = 1;
    g->lives = 3;
    br_level(g);
}

static void br_update(void* s, GameCtx* ctx) {
    Brk* g = s;
    float pv = 3.0f;
    if(ctx->held & KEY_BIT(InputKeyLeft)) g->pad -= pv;
    if(ctx->held & KEY_BIT(InputKeyRight)) g->pad += pv;
    g->pad = clampf(g->pad, 0, SCREEN_W - BR_PADW);
    if(!g->launched) {
        g->bx = g->pad + BR_PADW / 2;
        return;
    }
    for(int step = 0; step < 2; step++) {
        g->bx += g->vx;
        g->by += g->vy;
        if(g->bx < 2) g->bx = 2, g->vx = -g->vx;
        if(g->bx > SCREEN_W - 2) g->bx = SCREEN_W - 2, g->vx = -g->vx;
        if(g->by < BR_TOP - 6) g->by = BR_TOP - 6, g->vy = -g->vy;
        int py = SCREEN_H - 4;
        if(g->vy > 0 && g->by >= py - 1 && g->by <= py + 2 && g->bx >= g->pad - 1 &&
           g->bx <= g->pad + BR_PADW + 1) {
            g->vy = -g->vy;
            g->by = py - 1;
            float hit = (g->bx - (g->pad + BR_PADW / 2)) / (BR_PADW / 2);
            g->vx = clampf(hit * 2.2f, -2.4f, 2.4f);
            ctx->sfx |= SFX_BLIP;
        }
        for(int r = 0; r < BR_ROWS; r++)
            for(int cc = 0; cc < BR_COLS; cc++) {
                if(!g->brick[r][cc]) continue;
                int bxp = cc * BR_BW + 1, byp = BR_TOP + r * (BR_BH + 1);
                if(g->bx >= bxp - 1 && g->bx <= bxp + BR_BW && g->by >= byp - 1 &&
                   g->by <= byp + BR_BH) {
                    g->brick[r][cc] = 0;
                    g->alive--;
                    g->vy = -g->vy;
                    ctx->score += 1;
                    ctx->sfx |= SFX_BLIP;
                    goto done;
                }
            }
    done:;
        if(g->by > SCREEN_H) {
            g->lives--;
            ctx->sfx |= SFX_HIT;
            if(g->lives <= 0) {
                ctx->over = true;
                ctx->sfx |= SFX_GAME_OVER;
                return;
            }
            br_level(g);
            /* keep bricks: reset ball only */
            g->alive = 0;
            for(int r = 0; r < BR_ROWS; r++)
                for(int cc = 0; cc < BR_COLS; cc++) g->alive += g->brick[r][cc];
            return;
        }
    }
    if(g->alive <= 0) {
        g->level++;
        ctx->score += 10;
        ctx->sfx |= SFX_POWER;
        br_level(g);
    }
}

static void br_draw(void* s, Canvas* c, GameCtx* ctx) {
    Brk* g = s;
    canvas_draw_box(c, 0, 0, SCREEN_W, 10);
    canvas_set_color(c, ColorWhite);
    char buf[20];
    snprintf(buf, sizeof(buf), "Lvl %d  %ld", g->level, (long)ctx->score);
    canvas_draw_str(c, 2, 8, buf);
    for(int i = 0; i < g->lives; i++) canvas_draw_box(c, SCREEN_W - 4 - i * 5, 3, 3, 3);
    canvas_set_color(c, ColorBlack);
    for(int r = 0; r < BR_ROWS; r++)
        for(int cc = 0; cc < BR_COLS; cc++)
            if(g->brick[r][cc])
                canvas_draw_box(c, cc * BR_BW + 1, BR_TOP + r * (BR_BH + 1), BR_BW - 1, BR_BH);
    canvas_draw_box(c, (int)g->pad, SCREEN_H - 3, BR_PADW, 3);
    canvas_draw_disc(c, (int)g->bx, (int)g->by, 1);
    if(!g->launched) draw_str_boxed(c, SCREEN_W / 2, 40, AlignCenter, "OK to launch");
}

static void br_input(void* s, GameCtx* ctx, InputKey key, InputType type) {
    (void)ctx;
    Brk* g = s;
    if(key == InputKeyOk && type == InputTypePress && !g->launched) {
        g->launched = true;
        g->vy = -2.2f;
        g->vx = 1.0f;
    }
}

const GameDef game_breakout = {
    .name = "Breakout",
    .icon = &spr_br_icon,
    .state_size = sizeof(Brk),
    .init = br_init,
    .update = br_update,
    .draw = br_draw,
    .input = br_input,
};
