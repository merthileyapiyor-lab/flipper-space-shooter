#include "../common.h"
#include <math.h>

#define SH_HUD 9
#define SH_BULLETS 10
#define SH_EBULLETS 20
#define SH_ENEMIES 12
#define SH_PARTS 32

typedef struct {
    float x, y, vx, vy;
    bool on;
} SBullet;

typedef struct {
    uint8_t type; /* 0 drift 1 shoot 2 dive 3 boss */
    float x, y, vx, vy, base;
    int t, hp, maxhp, cd;
    uint8_t flash;
    bool on;
} SEnemy;

typedef struct {
    float x, y, vx, vy;
    int8_t life;
} SPart;

typedef struct {
    float px, py;
    int lives, wave, to_spawn, spawn_t, banner, fire_cd, invuln, shake;
    SBullet b[SH_BULLETS];
    SBullet eb[SH_EBULLETS];
    SEnemy e[SH_ENEMIES];
    SPart p[SH_PARTS];
    float starx[16], stary[16], stars[16];
} Shooter;

static const char* const sh_ship[] = {"..#..", ".###.", "#####", "#.#.#"};
static const Sprite spr_sh_ship = {5, 4, sh_ship};
static const char* const sh_drift[] = {"#...#", ".###.", "##.##", "#.#.#"};
static const Sprite spr_sh_drift = {5, 4, sh_drift};
static const char* const sh_shoot[] = {".###.", "#####", "#.#.#", ".#.#."};
static const Sprite spr_sh_shoot = {5, 4, sh_shoot};
static const char* const sh_dive[] = {"#...#", "##.##", ".###.", "..#.."};
static const Sprite spr_sh_dive = {5, 4, sh_dive};
static const char* const sh_boss[] = {
    "..############..", ".##############.", "###.##.##.##.###",
    "################", ".##..######..##.", "#.#..#....#..#.#"};
static const Sprite spr_sh_boss = {16, 6, sh_boss};
static const char* const sh_icon[] = {"..#..", ".###.", "#####", "#.#.#", "....."};
static const Sprite spr_sh_icon = {5, 5, sh_icon};

static const Sprite* sh_spr(int t) {
    return t == 1 ? &spr_sh_shoot : t == 2 ? &spr_sh_dive : t == 3 ? &spr_sh_boss : &spr_sh_drift;
}

static void sh_parts(Shooter* g, float x, float y, int n) {
    for(int i = 0; i < SH_PARTS && n; i++) {
        if(g->p[i].life > 0) continue;
        float a = (float)(i * 37 % 628) / 100.0f;
        g->p[i].x = x;
        g->p[i].y = y;
        g->p[i].vx = cosf(a) * 1.3f;
        g->p[i].vy = sinf(a) * 1.3f;
        g->p[i].life = 9;
        n--;
    }
}

static void sh_wave(Shooter* g) {
    g->banner = 50;
    g->spawn_t = 40;
    g->to_spawn = (g->wave % 5 == 0) ? 1 : clampi(4 + g->wave * 2, 4, 24);
}

static void sh_init(void* s, GameCtx* ctx) {
    Shooter* g = s;
    g->px = SCREEN_W / 2;
    g->py = SCREEN_H - 8;
    g->lives = 3;
    g->wave = 1;
    g->invuln = 60;
    for(int i = 0; i < 16; i++) {
        g->starx[i] = rndi(ctx, SCREEN_W);
        g->stary[i] = SH_HUD + rndi(ctx, SCREEN_H - SH_HUD);
        g->stars[i] = 0.3f + rndf(ctx) * 0.8f;
    }
    sh_wave(g);
}

static void sh_fire_e(Shooter* g, float x, float y, float vx, float vy) {
    for(int i = 0; i < SH_EBULLETS; i++)
        if(!g->eb[i].on) {
            g->eb[i] = (SBullet){x, y, vx, vy, true};
            return;
        }
}

static void sh_spawn(Shooter* g, GameCtx* ctx) {
    SEnemy* e = NULL;
    for(int i = 0; i < SH_ENEMIES; i++)
        if(!g->e[i].on) {
            e = &g->e[i];
            break;
        }
    if(!e) return;
    memset(e, 0, sizeof(*e));
    e->on = true;
    g->to_spawn--;
    if(g->wave % 5 == 0) {
        e->type = 3;
        e->maxhp = e->hp = 18 + g->wave * 2;
        e->x = SCREEN_W / 2;
        e->y = -6;
        e->cd = 50;
        ctx->sfx |= SFX_ALARM;
        return;
    }
    int r = rndi(ctx, 100);
    int dive = g->wave >= 3 ? clampi(15 + g->wave, 0, 35) : 0;
    e->type = (r < dive) ? 2 : (g->wave >= 2 && r < dive + 30) ? 1 : 0;
    e->y = SH_HUD - 4;
    e->hp = e->maxhp = (e->type == 1) ? 2 : 1;
    e->x = 8 + rndi(ctx, SCREEN_W - 16);
    e->base = e->x;
    if(e->type == 0)
        e->vy = 0.3f + g->wave * 0.03f, e->t = rndi(ctx, 200);
    else if(e->type == 1)
        e->vx = rndi(ctx, 2) ? 0.6f : -0.6f, e->cd = 40 + rndi(ctx, 40);
}

static void sh_hit(Shooter* g, GameCtx* ctx) {
    if(g->invuln > 0) return;
    g->lives--;
    g->invuln = 80;
    g->shake = 8;
    sh_parts(g, g->px, g->py + 2, 16);
    ctx->sfx |= SFX_HIT;
    if(g->lives <= 0) {
        ctx->over = true;
        ctx->score = (g->wave - 1) * 100 + ctx->score;
    }
}

static void sh_kill(Shooter* g, GameCtx* ctx, SEnemy* e) {
    const Sprite* s = sh_spr(e->type);
    e->on = false;
    sh_parts(g, e->x, e->y + s->h / 2, e->type == 3 ? 24 : 8);
    ctx->score += e->type == 3 ? 200 : e->type == 0 ? 10 : 20;
    ctx->sfx |= e->type == 3 ? SFX_POWER : SFX_BLIP;
    if(e->type == 3) g->shake = 12;
}

static void sh_update(void* s, GameCtx* ctx) {
    Shooter* g = s;
    if(g->shake) g->shake--;
    for(int i = 0; i < 16; i++) {
        g->stary[i] += g->stars[i];
        if(g->stary[i] >= SCREEN_H) g->stary[i] = SH_HUD, g->starx[i] = rndi(ctx, SCREEN_W);
    }
    for(int i = 0; i < SH_PARTS; i++)
        if(g->p[i].life > 0) g->p[i].x += g->p[i].vx, g->p[i].y += g->p[i].vy, g->p[i].life--;

    float sp = 1.7f;
    if(ctx->held & KEY_BIT(InputKeyLeft)) g->px -= sp;
    if(ctx->held & KEY_BIT(InputKeyRight)) g->px += sp;
    if(ctx->held & KEY_BIT(InputKeyUp)) g->py -= sp * 0.7f;
    if(ctx->held & KEY_BIT(InputKeyDown)) g->py += sp * 0.7f;
    g->px = clampf(g->px, 3, SCREEN_W - 4);
    g->py = clampf(g->py, 30, SCREEN_H - 5);

    if(g->fire_cd) g->fire_cd--;
    if(g->fire_cd == 0) {
        for(int i = 0; i < SH_BULLETS; i++)
            if(!g->b[i].on) {
                g->b[i] = (SBullet){g->px, g->py - 2, 0, -3.3f, true};
                break;
            }
        g->fire_cd = 7;
    }
    if(g->invuln) g->invuln--;
    if(g->banner) g->banner--;

    if(g->to_spawn > 0) {
        if(g->spawn_t > 0)
            g->spawn_t--;
        else
            sh_spawn(g, ctx), g->spawn_t = clampi(35 - g->wave * 2, 12, 35);
    }

    for(int i = 0; i < SH_BULLETS; i++) {
        SBullet* b = &g->b[i];
        if(!b->on) continue;
        b->y += b->vy;
        if(b->y < SH_HUD - 3) {
            b->on = false;
            continue;
        }
        for(int j = 0; j < SH_ENEMIES; j++) {
            SEnemy* e = &g->e[j];
            if(!e->on) continue;
            const Sprite* sp2 = sh_spr(e->type);
            if(overlap(b->x, b->y, 1, 3, e->x - sp2->w / 2.0f, e->y, sp2->w, sp2->h)) {
                b->on = false;
                e->hp--;
                e->flash = 2;
                if(e->hp <= 0) sh_kill(g, ctx, e);
                break;
            }
        }
    }

    float plx = g->px - 3, ply = g->py, plw = 6, plh = 5;
    for(int i = 0; i < SH_ENEMIES; i++) {
        SEnemy* e = &g->e[i];
        if(!e->on) continue;
        const Sprite* sp2 = sh_spr(e->type);
        e->t++;
        if(e->flash) e->flash--;
        float su = 1.0f + (g->wave - 1) * 0.05f;
        if(e->type == 0) {
            e->y += e->vy;
            e->x = e->base + sinf(e->t * 0.06f) * 10;
        } else if(e->type == 1) {
            if(e->y < SH_HUD + 4)
                e->y += 0.5f;
            else {
                e->x += e->vx;
                if(e->x < 6 || e->x > SCREEN_W - 6) e->vx = -e->vx;
                if(--e->cd <= 0)
                    e->cd = clampi(70 - g->wave * 3, 30, 99) + rndi(ctx, 20),
                    sh_fire_e(g, e->x, e->y + 4, 0, 1.1f * su);
            }
        } else if(e->type == 2) {
            if(e->t < 25)
                e->y += 0.5f;
            else if(e->t == 25)
                e->vx = clampf((g->px - e->x) * 0.03f, -1.3f, 1.3f), e->vy = 1.7f * su;
            else
                e->x += e->vx, e->y += e->vy;
        } else {
            if(e->y < SH_HUD + 2)
                e->y += 0.4f;
            else {
                e->x = SCREEN_W / 2 + sinf(e->t * 0.02f) * 42;
                if(--e->cd <= 0) {
                    e->cd = clampi(40 - g->wave, 20, 40);
                    sh_fire_e(g, e->x - 6, e->y + 6, -0.5f, 1.2f);
                    sh_fire_e(g, e->x, e->y + 6, 0, 1.3f);
                    sh_fire_e(g, e->x + 6, e->y + 6, 0.5f, 1.2f);
                }
            }
        }
        if(e->y > SCREEN_H) {
            e->on = false;
            sh_hit(g, ctx);
        } else if(e->x < -18 || e->x > SCREEN_W + 18)
            e->on = false;
        else if(overlap(plx, ply, plw, plh, e->x - sp2->w / 2.0f, e->y, sp2->w, sp2->h)) {
            bool vuln = g->invuln == 0;
            sh_hit(g, ctx);
            if(vuln && e->type != 3) e->on = false;
        }
    }

    for(int i = 0; i < SH_EBULLETS; i++) {
        SBullet* b = &g->eb[i];
        if(!b->on) continue;
        b->x += b->vx;
        b->y += b->vy;
        if(b->y > SCREEN_H || b->x < -2 || b->x > SCREEN_W + 2)
            b->on = false;
        else if(overlap(b->x - 1, b->y - 1, 2, 2, plx, ply, plw, plh)) {
            if(g->invuln == 0) b->on = false;
            sh_hit(g, ctx);
        }
    }

    if(g->to_spawn == 0 && g->banner == 0) {
        bool any = false;
        for(int i = 0; i < SH_ENEMIES; i++)
            if(g->e[i].on) any = true;
        if(!any) g->wave++, sh_wave(g), ctx->score += 50;
    }
}

static void sh_draw(void* s, Canvas* c, GameCtx* ctx) {
    Shooter* g = s;
    int ox = g->shake ? (g->shake % 2 ? 1 : -1) : 0;
    for(int i = 0; i < 16; i++) canvas_draw_dot(c, (int)g->starx[i], (int)g->stary[i]);

    for(int i = 0; i < SH_ENEMIES; i++) {
        SEnemy* e = &g->e[i];
        if(!e->on) continue;
        const Sprite* sp = sh_spr(e->type);
        int x = (int)(e->x - sp->w / 2.0f) + ox, y = (int)e->y;
        if(e->flash)
            for(int r = 0; r < sp->h; r++) {
                if(y + r < SH_HUD) continue;
                canvas_draw_line(c, x, y + r, x + sp->w - 1, y + r);
            }
        else
            draw_sprite(c, x, y, sp, SH_HUD);
        if(e->type == 3) {
            int w = (SCREEN_W - 40) * e->hp / e->maxhp;
            canvas_draw_frame(c, 19, SH_HUD, SCREEN_W - 38, 3);
            if(w > 0) canvas_draw_box(c, 20, SH_HUD + 1, w, 1);
        }
    }
    for(int i = 0; i < SH_BULLETS; i++)
        if(g->b[i].on) canvas_draw_line(c, (int)g->b[i].x + ox, (int)g->b[i].y, (int)g->b[i].x + ox, (int)g->b[i].y + 2);
    for(int i = 0; i < SH_EBULLETS; i++)
        if(g->eb[i].on) canvas_draw_box(c, (int)g->eb[i].x - 1 + ox, (int)g->eb[i].y - 1, 2, 2);
    for(int i = 0; i < SH_PARTS; i++)
        if(g->p[i].life > 0) canvas_draw_dot(c, (int)g->p[i].x + ox, (int)g->p[i].y);

    if(!(g->invuln && (ctx->now_ms / 90) % 2)) draw_sprite(c, (int)g->px - 2 + ox, (int)g->py, &spr_sh_ship, SH_HUD);

    canvas_draw_box(c, 0, 0, SCREEN_W, SH_HUD);
    canvas_set_color(c, ColorWhite);
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", (long)ctx->score);
    canvas_draw_str(c, 2, 7, buf);
    snprintf(buf, sizeof(buf), "W%d", g->wave);
    canvas_draw_str_aligned(c, 58, 7, AlignCenter, AlignBottom, buf);
    for(int i = 0; i < g->lives; i++) {
        canvas_draw_box(c, SCREEN_W - 5 - i * 6, 2, 4, 4);
    }
    canvas_set_color(c, ColorBlack);

    if(g->banner > 0 && (g->banner > 15 || (ctx->now_ms / 100) % 2)) {
        snprintf(buf, sizeof(buf), g->wave % 5 == 0 ? "WAVE %d BOSS" : "WAVE %d", g->wave);
        draw_str_boxed(c, SCREEN_W / 2, 32, AlignCenter, buf);
    }
}

static void sh_input(void* s, GameCtx* ctx, InputKey key, InputType type) {
    (void)s;
    (void)ctx;
    (void)key;
    (void)type;
}

const GameDef game_shooter = {
    .name = "Space Shooter",
    .icon = &spr_sh_icon,
    .state_size = sizeof(Shooter),
    .init = sh_init,
    .update = sh_update,
    .draw = sh_draw,
    .input = sh_input,
};
