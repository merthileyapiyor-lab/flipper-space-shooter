#include "../common.h"

#define FL_PIPES 3
#define FL_GAP 24
#define FL_PW 10
#define FL_SPACE 52

typedef struct {
    float y, vy;
    float px[FL_PIPES];
    int gap[FL_PIPES];
    bool scored[FL_PIPES];
    bool started;
    int flap;
} Flap;

static const char* const fl_icon[] = {".##..", "####.", ".####", "####.", ".##.."};
static const Sprite spr_fl_icon = {5, 5, fl_icon};

static void fl_init(void* s, GameCtx* ctx) {
    Flap* g = s;
    g->y = SCREEN_H / 2;
    for(int i = 0; i < FL_PIPES; i++) {
        g->px[i] = SCREEN_W + 20 + i * FL_SPACE;
        g->gap[i] = 12 + rndi(ctx, SCREEN_H - FL_GAP - 20);
    }
}

static void fl_update(void* s, GameCtx* ctx) {
    Flap* g = s;
    if(g->flap) g->flap--;
    if(!g->started) return;
    g->vy += 0.32f;
    g->y += g->vy;
    if(g->y < 2) g->y = 2, g->vy = 0;
    if(g->y > SCREEN_H - 4) {
        ctx->over = true;
        ctx->sfx |= SFX_GAME_OVER;
        return;
    }
    for(int i = 0; i < FL_PIPES; i++) {
        g->px[i] -= 1.4f;
        if(g->px[i] < -FL_PW) {
            g->px[i] += FL_PIPES * FL_SPACE;
            g->gap[i] = 12 + rndi(ctx, SCREEN_H - FL_GAP - 20);
            g->scored[i] = false;
        }
        if(!g->scored[i] && g->px[i] + FL_PW < 24) {
            g->scored[i] = true;
            ctx->score += 1;
            ctx->sfx |= SFX_BLIP;
        }
        float bx = 24;
        if(bx + 3 > g->px[i] && bx - 3 < g->px[i] + FL_PW) {
            if(g->y - 3 < g->gap[i] || g->y + 3 > g->gap[i] + FL_GAP) {
                ctx->over = true;
                ctx->sfx |= SFX_GAME_OVER;
                return;
            }
        }
    }
}

static void fl_draw(void* s, Canvas* c, GameCtx* ctx) {
    Flap* g = s;
    for(int i = 0; i < FL_PIPES; i++) {
        int x = (int)g->px[i];
        canvas_draw_box(c, x, 0, FL_PW, g->gap[i]);
        int by = g->gap[i] + FL_GAP;
        canvas_draw_box(c, x, by, FL_PW, SCREEN_H - by);
    }
    int by = (int)g->y;
    if(g->flap) {
        canvas_draw_box(c, 21, by - 1, 6, 3);
        canvas_draw_line(c, 20, by - 3, 23, by - 3);
    } else {
        canvas_draw_box(c, 21, by - 2, 6, 4);
    }
    canvas_draw_dot(c, 26, by - 1);
    char buf[8];
    snprintf(buf, sizeof(buf), "%ld", (long)ctx->score);
    draw_str_boxed(c, SCREEN_W - 4, 10, AlignRight, buf);
    if(!g->started) draw_str_boxed(c, SCREEN_W / 2, 36, AlignCenter, "OK to flap");
}

static void fl_input(void* s, GameCtx* ctx, InputKey key, InputType type) {
    Flap* g = s;
    if((key == InputKeyOk || key == InputKeyUp) && type == InputTypePress) {
        g->started = true;
        g->vy = -2.6f;
        g->flap = 4;
        ctx->sfx |= SFX_TICK;
    }
}

const GameDef game_flappy = {
    .name = "Flappy",
    .icon = &spr_fl_icon,
    .state_size = sizeof(Flap),
    .init = fl_init,
    .update = fl_update,
    .draw = fl_draw,
    .input = fl_input,
};
