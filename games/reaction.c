#include "../common.h"

/* Wait for the screen to flash, then hit OK as fast as you can.
   Score = reaction time in ms, averaged over 5 rounds. Lower is better. */

#define RC_ROUNDS 5

typedef enum { RC_Ready, RC_Waiting, RC_Go, RC_Result, RC_TooSoon } RcPhase;

typedef struct {
    RcPhase phase;
    uint32_t phase_t0;
    uint32_t wait_ms; /* random delay before GO */
    uint32_t go_ms;
    int round;
    uint32_t total;
    int last; /* last reaction ms */
} React;

static const char* const rc_icon[] = {"..#..", ".###.", "#.#.#", "..#..", "..#.."};
static const Sprite spr_rc_icon = {5, 5, rc_icon};

static void rc_arm(React* g, GameCtx* ctx) {
    g->phase = RC_Waiting;
    g->phase_t0 = ctx->now_ms;
    g->wait_ms = 900 + rndi(ctx, 2600);
}

static void rc_init(void* s, GameCtx* ctx) {
    (void)ctx;
    React* g = s;
    g->phase = RC_Ready;
}

static void rc_update(void* s, GameCtx* ctx) {
    React* g = s;
    if(g->phase == RC_Waiting && ctx->now_ms - g->phase_t0 >= g->wait_ms) {
        g->phase = RC_Go;
        g->go_ms = ctx->now_ms;
        ctx->sfx |= SFX_BLIP;
    }
}

static void rc_draw(void* s, Canvas* c, GameCtx* ctx) {
    (void)ctx;
    React* g = s;
    canvas_draw_str(c, 2, 8, "Reaction");
    char buf[20];
    snprintf(buf, sizeof(buf), "%d/%d", g->round, RC_ROUNDS);
    canvas_draw_str_aligned(c, SCREEN_W - 2, 8, AlignRight, AlignBottom, buf);

    if(g->phase == RC_Go) {
        canvas_draw_box(c, 0, 10, SCREEN_W, SCREEN_H - 10);
        canvas_set_color(c, ColorWhite);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, SCREEN_W / 2, 40, AlignCenter, AlignBottom, "HIT OK!");
        canvas_set_font(c, FontSecondary);
        canvas_set_color(c, ColorBlack);
        return;
    }
    canvas_set_font(c, FontPrimary);
    const char* msg = "";
    switch(g->phase) {
    case RC_Ready:
        msg = "OK to start";
        break;
    case RC_Waiting:
        msg = "Wait...";
        break;
    case RC_TooSoon:
        msg = "Too soon!";
        break;
    case RC_Result: {
        snprintf(buf, sizeof(buf), "%d ms", g->last);
        canvas_draw_str_aligned(c, SCREEN_W / 2, 34, AlignCenter, AlignBottom, buf);
        canvas_set_font(c, FontSecondary);
        canvas_draw_str_aligned(c, SCREEN_W / 2, 50, AlignCenter, AlignBottom,
                                g->round >= RC_ROUNDS ? "Done!" : "OK for next");
        canvas_set_font(c, FontPrimary);
        msg = NULL;
        break;
    }
    default:
        break;
    }
    if(msg) canvas_draw_str_aligned(c, SCREEN_W / 2, 40, AlignCenter, AlignBottom, msg);
    canvas_set_font(c, FontSecondary);
    if(g->round > 0 && g->phase != RC_Result) {
        snprintf(buf, sizeof(buf), "avg %lu ms", (unsigned long)(g->total / g->round));
        canvas_draw_str_aligned(c, SCREEN_W / 2, 62, AlignCenter, AlignBottom, buf);
    }
}

static void rc_input(void* s, GameCtx* ctx, InputKey key, InputType type) {
    React* g = s;
    if(key != InputKeyOk || type != InputTypePress) return;
    switch(g->phase) {
    case RC_Ready:
        rc_arm(g, ctx);
        break;
    case RC_Waiting: /* jumped the gun */
        g->phase = RC_TooSoon;
        ctx->sfx |= SFX_HIT;
        break;
    case RC_Go: {
        g->last = (int)(ctx->now_ms - g->go_ms);
        g->total += g->last;
        g->round++;
        g->phase = RC_Result;
        ctx->sfx |= SFX_TICK;
        if(g->round >= RC_ROUNDS) {
            ctx->score = g->total / RC_ROUNDS;
            ctx->won = true;
            ctx->over = true;
            ctx->sfx |= SFX_WIN;
        }
        break;
    }
    case RC_Result:
        rc_arm(g, ctx);
        break;
    case RC_TooSoon:
        rc_arm(g, ctx);
        break;
    }
}

const GameDef game_reaction = {
    .name = "Reaction",
    .icon = &spr_rc_icon,
    .state_size = sizeof(React),
    .lower_is_better = true,
    .unit = "ms",
    .init = rc_init,
    .update = rc_update,
    .draw = rc_draw,
    .input = rc_input,
};
