#include "../common.h"

#define DN_GROUND 52
#define DN_OBST 4

typedef struct {
    float y, vy;
    bool jumping, ducking, started;
    float ox[DN_OBST];
    uint8_t otype[DN_OBST]; /* 0 small cactus, 1 big cactus, 2 bird */
    float speed, dist;
    int anim;
} Dino;

static const char* const dn_icon[] = {".###.", ".####", "####.", ".#.#.", "....."};
static const Sprite spr_dn_icon = {5, 5, dn_icon};

static void dn_obst(Dino* g, GameCtx* ctx, int i, float base) {
    g->ox[i] = base + 40 + rndi(ctx, 50);
    g->otype[i] = rndi(ctx, 10) < 3 ? 2 : rndi(ctx, 2);
}

static void dn_init(void* s, GameCtx* ctx) {
    Dino* g = s;
    g->y = DN_GROUND;
    g->speed = 2.2f;
    for(int i = 0; i < DN_OBST; i++) dn_obst(g, ctx, i, SCREEN_W + i * 60);
}

static void dn_update(void* s, GameCtx* ctx) {
    Dino* g = s;
    g->anim++;
    if(!g->started) return;
    g->dist += g->speed;
    if((int)g->dist % 10 < (int)g->speed) ctx->score = (int)(g->dist / 10);
    if(g->speed < 5.0f) g->speed += 0.0015f;

    g->ducking = (ctx->held & KEY_BIT(InputKeyDown)) && !g->jumping;
    if(g->jumping) {
        g->vy += 0.6f;
        g->y += g->vy;
        if(g->y >= DN_GROUND) {
            g->y = DN_GROUND;
            g->jumping = false;
        }
    }
    int dx = 12;
    int dtop = g->ducking ? DN_GROUND - 4 : (int)g->y - 10;
    int dbot = DN_GROUND;
    for(int i = 0; i < DN_OBST; i++) {
        g->ox[i] -= g->speed;
        if(g->ox[i] < -10) {
            float mx = 0;
            for(int j = 0; j < DN_OBST; j++)
                if(g->ox[j] > mx) mx = g->ox[j];
            dn_obst(g, ctx, i, mx);
        }
        int ow = g->otype[i] == 1 ? 7 : 6;
        int otop = g->otype[i] == 2 ? 30 : DN_GROUND - 10;
        int obot = g->otype[i] == 2 ? 38 : DN_GROUND;
        if(g->ox[i] < dx + 8 && g->ox[i] + ow > dx && dtop < obot && dbot > otop) {
            ctx->over = true;
            ctx->sfx |= SFX_GAME_OVER;
            return;
        }
    }
}

static void dn_draw(void* s, Canvas* c, GameCtx* ctx) {
    Dino* g = s;
    canvas_draw_line(c, 0, DN_GROUND + 1, SCREEN_W, DN_GROUND + 1);
    for(int i = 0; i < SCREEN_W; i += 7)
        canvas_draw_dot(c, (i - (int)g->dist) % SCREEN_W + (((i - (int)g->dist) % SCREEN_W) < 0 ? SCREEN_W : 0), DN_GROUND + 3);
    int dx = 12;
    if(g->ducking) {
        canvas_draw_box(c, dx, DN_GROUND - 4, 10, 5);
        canvas_draw_dot(c, dx + 9, DN_GROUND - 3);
    } else {
        int by = (int)g->y;
        canvas_draw_box(c, dx, by - 10, 6, 8);
        canvas_draw_box(c, dx + 4, by - 11, 4, 4);
        canvas_draw_dot(c, dx + 7, by - 10);
        if(!g->jumping && (g->anim / 5) % 2)
            canvas_draw_line(c, dx + 1, by - 1, dx + 1, by + 1);
        else if(!g->jumping)
            canvas_draw_line(c, dx + 4, by - 1, dx + 4, by + 1);
    }
    for(int i = 0; i < DN_OBST; i++) {
        int x = (int)g->ox[i];
        if(x < -10 || x > SCREEN_W) continue;
        if(g->otype[i] == 2) {
            int wy = 34 + ((g->anim / 6) % 2 ? 0 : 2);
            canvas_draw_box(c, x, wy, 6, 2);
            canvas_draw_line(c, x, wy - 1, x + 2, wy - 1);
        } else {
            int h = g->otype[i] == 1 ? 12 : 9;
            int w = g->otype[i] == 1 ? 6 : 4;
            canvas_draw_box(c, x + 1, DN_GROUND - h, w, h);
            canvas_draw_line(c, x, DN_GROUND - h + 3, x, DN_GROUND - h + 5);
            canvas_draw_line(c, x + w + 1, DN_GROUND - h + 2, x + w + 1, DN_GROUND - h + 4);
        }
    }
    char buf[8];
    snprintf(buf, sizeof(buf), "%ld", (long)ctx->score);
    canvas_draw_str_aligned(c, SCREEN_W - 2, 8, AlignRight, AlignBottom, buf);
    if(!g->started) draw_str_boxed(c, SCREEN_W / 2, 24, AlignCenter, "OK jump, Down duck");
}

static void dn_input(void* s, GameCtx* ctx, InputKey key, InputType type) {
    Dino* g = s;
    if((key == InputKeyOk || key == InputKeyUp) && type == InputTypePress) {
        g->started = true;
        if(!g->jumping) {
            g->jumping = true;
            g->vy = -5.2f;
            ctx->sfx |= SFX_TICK;
        }
    }
}

const GameDef game_dino = {
    .name = "Dino Run",
    .icon = &spr_dn_icon,
    .state_size = sizeof(Dino),
    .init = dn_init,
    .update = dn_update,
    .draw = dn_draw,
    .input = dn_input,
};
