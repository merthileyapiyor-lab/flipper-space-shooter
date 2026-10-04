#include "game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ---------- sprites ---------- */

typedef struct {
    uint8_t w, h;
    const char* const* rows;
} Sprite;

static const char* const ship_rows[] = {
    "....#....",
    "...###...",
    "...#.#...",
    "..#####..",
    ".#######.",
    "##.###.##",
    "#...#...#",
};
static const Sprite spr_ship = {9, 7, ship_rows};

static const char* const drifter_rows[] = {
    ".#...#.",
    "..###..",
    ".##.##.",
    "#######",
    "#.#.#.#",
};
static const Sprite spr_drifter = {7, 5, drifter_rows};

static const char* const shooter_rows[] = {
    "..#####..",
    ".#######.",
    "##.###.##",
    "#########",
    "..#...#..",
    ".#.....#.",
};
static const Sprite spr_shooter = {9, 6, shooter_rows};

static const char* const diver_rows[] = {
    "#.....#",
    "##...##",
    ".#####.",
    "..#.#..",
    "..###..",
    "...#...",
};
static const Sprite spr_diver = {7, 6, diver_rows};

static const char* const boss_rows[] = {
    "......############......",
    "...##################...",
    ".######################.",
    "###..###..####..###..###",
    "########################",
    "#.##.##.########.##.##.#",
    "########################",
    ".###.##############.###.",
    "..##..##.##..##.##..##..",
    ".##....#.#....#.#....##.",
    "##.....##......##.....##",
    "#.......#......#.......#",
};
static const Sprite spr_boss = {24, 12, boss_rows};

static const char* const heart_rows[] = {
    ".#.#.",
    "#####",
    "#####",
    ".###.",
    "..#..",
};
static const Sprite spr_heart = {5, 5, heart_rows};

/* 3x5 glyphs shown inside power-up capsules */
static const char* const glyph_d[] = {"##.", "#.#", "#.#", "#.#", "##."};
static const char* const glyph_s[] = {".##", "#..", ".#.", "..#", "##."};
static const char* const glyph_plus[] = {"...", ".#.", "###", ".#.", "..."};
static const Sprite spr_glyphs[] = {{3, 5, glyph_d}, {3, 5, glyph_s}, {3, 5, glyph_plus}};

static const Sprite* enemy_sprite(EnemyType t) {
    switch(t) {
    case EnemyShooter:
        return &spr_shooter;
    case EnemyDiver:
        return &spr_diver;
    case EnemyBoss:
        return &spr_boss;
    default:
        return &spr_drifter;
    }
}

static void draw_sprite(Canvas* c, int x, int y, const Sprite* s, int min_y) {
    for(int row = 0; row < s->h; row++) {
        int py = y + row;
        if(py < min_y || py >= SCREEN_H) continue;
        const char* line = s->rows[row];
        for(int col = 0; col < s->w; col++) {
            int px = x + col;
            if(px < 0 || px >= SCREEN_W) continue;
            if(line[col] == '#') canvas_draw_dot(c, px, py);
        }
    }
}

/* ---------- helpers ---------- */

static uint32_t rnd(Game* g) {
    uint32_t x = g->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g->rng = x;
    return x;
}

static float rndf(Game* g) {
    return (float)(rnd(g) & 0xFFFF) / 65536.0f;
}

static int rndi(Game* g, int n) {
    return (int)(rnd(g) % (uint32_t)n);
}

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static bool overlap(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh) {
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

static void spawn_particles(Game* g, float x, float y, int count, float speed) {
    for(int i = 0; i < MAX_PARTICLES && count > 0; i++) {
        Particle* p = &g->particles[i];
        if(p->life > 0) continue;
        float a = rndf(g) * 6.2831853f;
        float s = speed * (0.3f + rndf(g));
        p->x = x;
        p->y = y;
        p->vx = cosf(a) * s;
        p->vy = sinf(a) * s;
        p->life = (int8_t)(8 + rndi(g, 10));
        count--;
    }
}

static void fire_enemy_bullet(Game* g, float x, float y, float vx, float vy) {
    for(int i = 0; i < MAX_EBULLETS; i++) {
        Bullet* b = &g->ebullets[i];
        if(b->active) continue;
        b->x = x;
        b->y = y;
        b->vx = vx;
        b->vy = vy;
        b->active = true;
        return;
    }
}

static void fire_player_bullet(Game* g, float x, float y) {
    for(int i = 0; i < MAX_BULLETS; i++) {
        Bullet* b = &g->bullets[i];
        if(b->active) continue;
        b->x = x;
        b->y = y;
        b->vx = 0;
        b->vy = -3.5f;
        b->active = true;
        return;
    }
}

static void init_stars(Game* g) {
    for(int i = 0; i < NUM_STARS; i++) {
        g->stars[i].x = (float)rndi(g, SCREEN_W);
        g->stars[i].y = (float)(HUD_H + rndi(g, SCREEN_H - HUD_H));
        g->stars[i].speed = 0.2f + rndf(g) * 0.9f;
    }
}

static void update_stars(Game* g) {
    for(int i = 0; i < NUM_STARS; i++) {
        Star* s = &g->stars[i];
        s->y += s->speed;
        if(s->y >= SCREEN_H) {
            s->y = HUD_H;
            s->x = (float)rndi(g, SCREEN_W);
        }
    }
}

static void start_wave(Game* g) {
    g->banner_timer = 60;
    g->spawn_timer = 60;
    if(g->wave % 5 == 0) {
        g->to_spawn = 1;
    } else {
        int n = 5 + g->wave * 2;
        g->to_spawn = n > 30 ? 30 : n;
    }
}

/* ---------- public ---------- */

void game_init(Game* g, uint32_t seed, uint32_t high_score) {
    memset(g, 0, sizeof(Game));
    g->rng = seed ? seed : 0x1234567u;
    g->high_score = high_score;
    g->state = StateTitle;
    init_stars(g);
}

void game_start(Game* g) {
    uint32_t rng = g->rng, hs = g->high_score, frame = g->frame;
    Star stars[NUM_STARS];
    memcpy(stars, g->stars, sizeof(stars));
    memset(g, 0, sizeof(Game));
    g->rng = rng;
    g->high_score = hs;
    g->frame = frame;
    memcpy(g->stars, stars, sizeof(stars));

    g->state = StatePlaying;
    g->px = SCREEN_W / 2;
    g->py = SCREEN_H - 9;
    g->lives = 3;
    g->wave = 1;
    g->invuln_timer = 60;
    start_wave(g);
}

static void spawn_enemy(Game* g) {
    Enemy* e = NULL;
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(!g->enemies[i].active) {
            e = &g->enemies[i];
            break;
        }
    }
    if(!e) return; /* field full; retry next spawn tick */

    memset(e, 0, sizeof(Enemy));
    e->active = true;
    g->to_spawn--;

    if(g->wave % 5 == 0) {
        e->type = EnemyBoss;
        e->max_hp = e->hp = (int16_t)(20 + g->wave * 3);
        e->x = SCREEN_W / 2;
        e->y = -12;
        e->target_y = HUD_H + 6; /* leave room for the hp bar */
        e->fire_cd = 60;
        g->sfx |= SFX_BOSS;
        return;
    }

    int r = rndi(g, 100);
    int diver_pct = g->wave >= 3 ? (20 + g->wave * 2 > 35 ? 35 : 20 + g->wave * 2) : 0;
    if(r < diver_pct) {
        e->type = EnemyDiver;
    } else if(g->wave >= 2 && r < diver_pct + 30) {
        e->type = EnemyShooter;
    } else {
        e->type = EnemyDrifter;
    }

    const Sprite* s = enemy_sprite(e->type);
    e->y = HUD_H - s->h;
    switch(e->type) {
    case EnemyDrifter:
        e->hp = 1;
        e->base_x = 18 + rndi(g, SCREEN_W - 36);
        e->x = e->base_x;
        e->vy = 0.35f + g->wave * 0.03f;
        e->t = rndi(g, 200);
        break;
    case EnemyShooter:
        e->hp = 2;
        e->x = 8 + rndi(g, SCREEN_W - 16);
        e->target_y = HUD_H + 3 + rndi(g, 14);
        e->vx = rndi(g, 2) ? 0.6f : -0.6f;
        e->fire_cd = 40 + rndi(g, 40);
        break;
    case EnemyDiver:
        e->hp = 1;
        e->x = 8 + rndi(g, SCREEN_W - 16);
        break;
    default:
        break;
    }
    e->max_hp = e->hp;
}

static void record_high_score(Game* g) {
    if(g->score > g->high_score) {
        g->high_score = g->score;
        g->new_high = true;
        g->save_pending = true;
    }
}

static void game_over(Game* g) {
    g->state = StateGameOver;
    g->sfx |= SFX_GAME_OVER;
    g->held_left = g->held_right = g->held_up = g->held_down = g->held_ok = false;
    record_high_score(g);
}

static void lose_life(Game* g) {
    g->lives--;
    g->shake = 8;
    g->sfx |= SFX_HIT;
    if(g->lives <= 0) game_over(g);
}

static void player_hit(Game* g) {
    if(g->invuln_timer > 0) return;
    if(g->shield_timer > 0) {
        g->shield_timer = 0;
        g->invuln_timer = 30;
        spawn_particles(g, g->px, g->py + 3, 10, 1.2f);
        g->sfx |= SFX_HIT;
        return;
    }
    g->double_timer = 0;
    g->invuln_timer = 90;
    spawn_particles(g, g->px, g->py + 3, 20, 1.6f);
    lose_life(g);
}

/* An enemy slipped past the bottom of the screen: costs a life, shield or not */
static void enemy_escaped(Game* g, float x) {
    spawn_particles(g, clampf(x, 2, SCREEN_W - 3), SCREEN_H - 2, 12, 1.2f);
    lose_life(g);
}

static void maybe_drop_powerup(Game* g, float x, float y, bool guaranteed) {
    if(!guaranteed && rndi(g, 100) >= 9) return;
    for(int i = 0; i < MAX_POWERUPS; i++) {
        PowerUp* p = &g->powerups[i];
        if(p->active) continue;
        p->active = true;
        p->x = x;
        p->y = y;
        if(guaranteed) {
            p->type = PowerLife;
        } else {
            int r = rndi(g, 100);
            p->type = r < 50 ? PowerDouble : (r < 85 ? PowerShield : PowerLife);
        }
        return;
    }
}

static void kill_enemy(Game* g, Enemy* e) {
    const Sprite* s = enemy_sprite(e->type);
    float cy = e->y + s->h / 2.0f;
    e->active = false;
    switch(e->type) {
    case EnemyBoss:
        g->score += 500 + g->wave * 20;
        spawn_particles(g, e->x - 6, cy, 20, 2.0f);
        spawn_particles(g, e->x + 6, cy, 20, 2.0f);
        g->shake = 15;
        g->sfx |= SFX_BOSS_KILL;
        maybe_drop_powerup(g, e->x, cy, true);
        break;
    case EnemyShooter:
        g->score += 25;
        break;
    case EnemyDiver:
        g->score += 20;
        break;
    default:
        g->score += 10;
        break;
    }
    if(e->type != EnemyBoss) {
        spawn_particles(g, e->x, cy, 8, 1.0f);
        g->sfx |= SFX_KILL;
        maybe_drop_powerup(g, e->x, cy, false);
    }
}

static void update_enemy(Game* g, Enemy* e) {
    const Sprite* s = enemy_sprite(e->type);
    float speedup = 1.0f + (g->wave - 1) * 0.05f;
    e->t++;
    if(e->flash) e->flash--;

    switch(e->type) {
    case EnemyDrifter:
        e->y += e->vy;
        e->x = e->base_x + sinf(e->t * 0.06f) * 12.0f;
        break;
    case EnemyShooter:
        if(e->y < e->target_y) {
            e->y += 0.5f;
        } else {
            e->x += e->vx;
            if(e->x < 6 || e->x > SCREEN_W - 6) e->vx = -e->vx;
            if(--e->fire_cd <= 0) {
                int cd = 70 - g->wave * 3;
                e->fire_cd = (int16_t)((cd < 30 ? 30 : cd) + rndi(g, 30));
                fire_enemy_bullet(g, e->x, e->y + s->h, 0, 1.1f * speedup);
            }
            /* creep down slowly so they can't camp forever */
            if(e->t % 240 == 0) e->target_y += 4;
        }
        break;
    case EnemyDiver:
        if(e->t < 30) {
            e->y += 0.5f;
        } else if(e->t == 30) {
            e->vx = clampf((g->px - e->x) * 0.025f, -1.2f, 1.2f);
            e->vy = 1.6f * speedup;
        } else {
            e->x += e->vx;
            e->y += e->vy;
        }
        break;
    case EnemyBoss:
        if(e->y < e->target_y) {
            e->y += 0.4f;
        } else {
            e->x = SCREEN_W / 2 + sinf(e->t * 0.02f) * 40.0f;
            if(--e->fire_cd <= 0) {
                float by = e->y + s->h;
                e->base_x += 1; /* volley counter */
                if(((int)e->base_x) % 3 == 0) {
                    float dx = g->px - e->x, dy = g->py - by;
                    float len = sqrtf(dx * dx + dy * dy);
                    if(len < 1) len = 1;
                    fire_enemy_bullet(g, e->x, by, dx / len * 1.5f, dy / len * 1.5f);
                } else {
                    fire_enemy_bullet(g, e->x - 8, by, -0.5f, 1.2f);
                    fire_enemy_bullet(g, e->x, by, 0, 1.3f);
                    fire_enemy_bullet(g, e->x + 8, by, 0.5f, 1.2f);
                }
                int cd = 45 - g->wave;
                e->fire_cd = (int16_t)(cd < 22 ? 22 : cd);
                /* enraged at low hp */
                if(e->hp * 3 < e->max_hp) e->fire_cd = (int16_t)(e->fire_cd * 2 / 3);
            }
        }
        break;
    }

    if(e->y > SCREEN_H) {
        e->active = false;
        enemy_escaped(g, e->x);
    } else if(e->x < -20 || e->x > SCREEN_W + 20) {
        e->active = false; /* flew off the side */
    }
}

void game_update(Game* g) {
    g->frame++;
    update_stars(g);
    if(g->shake) g->shake--;

    /* particles keep moving behind the game over screen */
    for(int i = 0; i < MAX_PARTICLES; i++) {
        Particle* p = &g->particles[i];
        if(p->life <= 0) continue;
        p->x += p->vx;
        p->y += p->vy;
        p->life--;
    }

    if(g->state != StatePlaying) return;

    /* player movement */
    const float speed = 1.7f;
    if(g->held_left) g->px -= speed;
    if(g->held_right) g->px += speed;
    if(g->held_up) g->py -= speed * 0.7f;
    if(g->held_down) g->py += speed * 0.7f;
    g->px = clampf(g->px, 4, SCREEN_W - 5);
    g->py = clampf(g->py, 34, SCREEN_H - 7);

    if(g->fire_cd) g->fire_cd--;
    if(g->fire_cd == 0) { /* auto-fire */
        if(g->double_timer > 0) {
            fire_player_bullet(g, g->px - 3, g->py + 1);
            fire_player_bullet(g, g->px + 3, g->py + 1);
        } else {
            fire_player_bullet(g, g->px, g->py - 2);
        }
        g->fire_cd = 6;
    }

    if(g->double_timer) g->double_timer--;
    if(g->shield_timer) g->shield_timer--;
    if(g->invuln_timer) g->invuln_timer--;
    if(g->banner_timer) g->banner_timer--;

    /* spawning */
    if(g->to_spawn > 0) {
        if(g->spawn_timer > 0) {
            g->spawn_timer--;
        } else {
            spawn_enemy(g);
            int st = 40 - g->wave * 3;
            g->spawn_timer = st < 12 ? 12 : st;
        }
    }

    /* player bullets */
    for(int i = 0; i < MAX_BULLETS; i++) {
        Bullet* b = &g->bullets[i];
        if(!b->active) continue;
        b->y += b->vy;
        if(b->y < HUD_H - 3) {
            b->active = false;
            continue;
        }
        for(int j = 0; j < MAX_ENEMIES; j++) {
            Enemy* e = &g->enemies[j];
            if(!e->active) continue;
            const Sprite* s = enemy_sprite(e->type);
            if(overlap(b->x, b->y, 1, 3, e->x - s->w / 2.0f, e->y, s->w, s->h)) {
                b->active = false;
                e->hp--;
                e->flash = 3;
                if(e->hp <= 0) kill_enemy(g, e);
                break;
            }
        }
    }

    /* enemies */
    const float pbx = g->px - 3, pby = g->py + 1, pbw = 7, pbh = 6; /* forgiving hitbox */
    for(int i = 0; i < MAX_ENEMIES; i++) {
        Enemy* e = &g->enemies[i];
        if(!e->active) continue;
        update_enemy(g, e);
        if(!e->active) continue;
        const Sprite* s = enemy_sprite(e->type);
        if(g->state == StatePlaying &&
           overlap(pbx, pby, pbw, pbh, e->x - s->w / 2.0f, e->y, s->w, s->h)) {
            bool was_vulnerable = g->invuln_timer == 0;
            player_hit(g);
            if(was_vulnerable && e->type != EnemyBoss) {
                e->active = false;
                spawn_particles(g, e->x, e->y + s->h / 2.0f, 8, 1.0f);
            }
        }
    }

    /* enemy bullets */
    for(int i = 0; i < MAX_EBULLETS; i++) {
        Bullet* b = &g->ebullets[i];
        if(!b->active) continue;
        b->x += b->vx;
        b->y += b->vy;
        if(b->y > SCREEN_H || b->x < -2 || b->x > SCREEN_W + 2) {
            b->active = false;
            continue;
        }
        if(g->state == StatePlaying && overlap(b->x - 1, b->y - 1, 2, 2, pbx, pby, pbw, pbh)) {
            if(g->invuln_timer == 0) b->active = false;
            player_hit(g);
        }
    }

    /* power-ups */
    for(int i = 0; i < MAX_POWERUPS; i++) {
        PowerUp* p = &g->powerups[i];
        if(!p->active) continue;
        p->y += 0.5f;
        if(p->y > SCREEN_H) {
            p->active = false;
            continue;
        }
        if(g->state == StatePlaying && overlap(p->x - 3, p->y - 3, 7, 7, pbx, pby, pbw, pbh)) {
            p->active = false;
            g->score += 50;
            g->sfx |= SFX_POWER;
            switch(p->type) {
            case PowerDouble:
                g->double_timer = 600;
                break;
            case PowerShield:
                g->shield_timer = 450;
                break;
            case PowerLife:
                if(g->lives < MAX_LIVES) g->lives++;
                break;
            }
        }
    }

    if(g->state != StatePlaying) return;

    /* wave complete? */
    if(g->to_spawn == 0 && g->banner_timer == 0) {
        bool any = false;
        for(int i = 0; i < MAX_ENEMIES; i++) {
            if(g->enemies[i].active) {
                any = true;
                break;
            }
        }
        if(!any) {
            g->wave++;
            start_wave(g);
        }
    }
}

/* ---------- drawing ---------- */

static void draw_box_centered(Canvas* c, int w, int h) {
    int x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2 + HUD_H / 2;
    canvas_set_color(c, ColorWhite);
    canvas_draw_box(c, x, y, w, h);
    canvas_set_color(c, ColorBlack);
    canvas_draw_rframe(c, x, y, w, h, 3);
}

static void draw_title(const Game* g, Canvas* c) {
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, SCREEN_W / 2, 14, AlignCenter, AlignBottom, "SPACE SHOOTER");

    draw_sprite(c, 30, 20, &spr_drifter, 0);
    draw_sprite(c, 60, 18, &spr_boss, 0);
    draw_sprite(c, 92, 20, &spr_diver, 0);
    draw_sprite(c, SCREEN_W / 2 - 4, 34, &spr_ship, 0);
    canvas_draw_line(c, SCREEN_W / 2, 31, SCREEN_W / 2, 33);

    canvas_set_font(c, FontSecondary);
    char buf[32];
    snprintf(buf, sizeof(buf), "Best: %lu", (unsigned long)g->high_score);
    canvas_draw_str_aligned(c, SCREEN_W / 2, 52, AlignCenter, AlignBottom, buf);
    if((g->frame / 15) % 2 == 0) {
        canvas_draw_str_aligned(c, SCREEN_W / 2, 63, AlignCenter, AlignBottom, "Press OK to start");
    }
}

static void draw_hud(const Game* g, Canvas* c) {
    canvas_set_color(c, ColorWhite);
    canvas_draw_box(c, 0, 0, SCREEN_W, HUD_H);
    canvas_set_color(c, ColorBlack);
    canvas_draw_line(c, 0, HUD_H - 1, SCREEN_W - 1, HUD_H - 1);

    canvas_set_font(c, FontSecondary);
    char buf[24];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)g->score);
    canvas_draw_str(c, 1, 7, buf);

    snprintf(buf, sizeof(buf), "W%d", g->wave);
    canvas_draw_str_aligned(c, 64, 7, AlignCenter, AlignBottom, buf);

    if(g->double_timer > 0 && (g->double_timer > 90 || (g->frame / 4) % 2)) {
        canvas_draw_str(c, 80, 7, "x2");
    }

    for(int i = 0; i < g->lives; i++) {
        draw_sprite(c, SCREEN_W - 6 - i * 6, 1, &spr_heart, 0);
    }
}

void game_draw(const Game* g, Canvas* canvas) {
    Canvas* c = canvas;
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);

    for(int i = 0; i < NUM_STARS; i++) {
        const Star* s = &g->stars[i];
        canvas_draw_dot(c, (int)s->x, (int)s->y);
        if(s->speed > 0.9f && s->y + 1 < SCREEN_H) canvas_draw_dot(c, (int)s->x, (int)s->y + 1);
    }

    if(g->state == StateTitle) {
        draw_title(g, c);
        return;
    }

    /* camera shake when hit */
    int ox = 0, oy = 0;
    if(g->shake) {
        ox = (g->shake % 2) ? 1 : -1;
        oy = (g->shake % 3) ? 0 : 1;
    }

    /* enemies */
    const Enemy* boss = NULL;
    for(int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy* e = &g->enemies[i];
        if(!e->active) continue;
        const Sprite* s = enemy_sprite(e->type);
        int x = (int)(e->x - s->w / 2.0f) + ox, y = (int)e->y + oy;
        if(e->type == EnemyBoss) boss = e;
        if(e->flash) {
            /* hit flash: solid silhouette */
            for(int r = 0; r < s->h; r++) {
                if(y + r < HUD_H) continue;
                canvas_draw_line(c, x, y + r, x + s->w - 1, y + r);
            }
        } else {
            draw_sprite(c, x, y, s, HUD_H);
        }
    }

    /* power-ups */
    for(int i = 0; i < MAX_POWERUPS; i++) {
        const PowerUp* p = &g->powerups[i];
        if(!p->active) continue;
        int x = (int)p->x - 3 + ox, y = (int)p->y - 3 + oy;
        canvas_draw_rframe(c, x - 1, y - 1, 9, 9, 2);
        draw_sprite(c, x + 2, y + 1, &spr_glyphs[p->type], HUD_H);
    }

    /* bullets */
    for(int i = 0; i < MAX_BULLETS; i++) {
        const Bullet* b = &g->bullets[i];
        if(!b->active) continue;
        canvas_draw_line(c, (int)b->x + ox, (int)b->y + oy, (int)b->x + ox, (int)b->y + 2 + oy);
    }
    for(int i = 0; i < MAX_EBULLETS; i++) {
        const Bullet* b = &g->ebullets[i];
        if(!b->active) continue;
        int x = (int)b->x - 1 + ox, y = (int)b->y - 1 + oy;
        canvas_draw_box(c, x, y, 2, 2);
    }

    /* particles */
    for(int i = 0; i < MAX_PARTICLES; i++) {
        const Particle* p = &g->particles[i];
        if(p->life <= 0) continue;
        int x = (int)p->x + ox, y = (int)p->y + oy;
        if(x >= 0 && x < SCREEN_W && y >= HUD_H && y < SCREEN_H) canvas_draw_dot(c, x, y);
    }

    /* player */
    if(g->state != StateGameOver) {
        bool blink = g->invuln_timer > 0 && (g->frame / 3) % 2;
        if(!blink) draw_sprite(c, (int)g->px - 4 + ox, (int)g->py + oy, &spr_ship, HUD_H);
        if(g->shield_timer > 0 && (g->shield_timer > 90 || (g->frame / 3) % 2)) {
            canvas_draw_circle(c, (int)g->px + ox, (int)g->py + 3 + oy, 7);
        }
        /* engine flicker */
        if(!blink && (g->frame / 2) % 2) {
            canvas_draw_dot(c, (int)g->px + ox, (int)g->py + 7 + oy);
        }
    }

    draw_hud(g, c);

    if(boss) {
        int w = (SCREEN_W - 40) * boss->hp / boss->max_hp;
        canvas_draw_frame(c, 19, HUD_H + 1, SCREEN_W - 38, 3);
        canvas_draw_line(c, 20, HUD_H + 2, 20 + w - 1, HUD_H + 2);
    }

    char buf[32];
    if(g->state == StatePlaying && g->banner_timer > 0 && (g->banner_timer > 20 || (g->frame / 3) % 2)) {
        canvas_set_font(c, FontPrimary);
        if(g->wave % 5 == 0) {
            snprintf(buf, sizeof(buf), "WAVE %d - BOSS!", g->wave);
        } else {
            snprintf(buf, sizeof(buf), "WAVE %d", g->wave);
        }
        int w = canvas_string_width(c, buf) + 10;
        canvas_set_color(c, ColorWhite);
        canvas_draw_box(c, (SCREEN_W - w) / 2, 24, w, 14);
        canvas_set_color(c, ColorBlack);
        canvas_draw_str_aligned(c, SCREEN_W / 2, 35, AlignCenter, AlignBottom, buf);
    }

    if(g->state == StatePaused) {
        draw_box_centered(c, 96, 34);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, SCREEN_W / 2, 30, AlignCenter, AlignBottom, "PAUSED");
        canvas_set_font(c, FontSecondary);
        canvas_draw_str_aligned(c, SCREEN_W / 2, 41, AlignCenter, AlignBottom, "OK: resume");
        canvas_draw_str_aligned(c, SCREEN_W / 2, 50, AlignCenter, AlignBottom, "Back: menu");
    } else if(g->state == StateGameOver) {
        draw_box_centered(c, 120, 44);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, SCREEN_W / 2, 26, AlignCenter, AlignBottom, "GAME OVER");
        canvas_set_font(c, FontSecondary);
        snprintf(buf, sizeof(buf), "Score: %lu  Wave %d", (unsigned long)g->score, g->wave);
        canvas_draw_str_aligned(c, SCREEN_W / 2, 36, AlignCenter, AlignBottom, buf);
        if(g->new_high) {
            if((g->frame / 8) % 2) {
                canvas_draw_str_aligned(c, SCREEN_W / 2, 45, AlignCenter, AlignBottom, "NEW HIGH SCORE!");
            }
        } else {
            snprintf(buf, sizeof(buf), "Best: %lu", (unsigned long)g->high_score);
            canvas_draw_str_aligned(c, SCREEN_W / 2, 45, AlignCenter, AlignBottom, buf);
        }
        canvas_draw_str_aligned(c, SCREEN_W / 2, 55, AlignCenter, AlignBottom, "OK: retry   Back: menu");
    }
}

/* ---------- input ---------- */

static void release_all(Game* g) {
    g->held_left = g->held_right = g->held_up = g->held_down = g->held_ok = false;
}

bool game_input(Game* g, InputKey key, InputType type) {
    if(key == InputKeyBack && type == InputTypeLong) {
        if(g->state == StatePlaying || g->state == StatePaused) record_high_score(g);
        return false;
    }

    switch(g->state) {
    case StateTitle:
        if(key == InputKeyOk && type == InputTypeShort) game_start(g);
        if(key == InputKeyBack && type == InputTypeShort) return false;
        break;

    case StatePlaying:
        if(type == InputTypePress || type == InputTypeRelease) {
            bool down = type == InputTypePress;
            switch(key) {
            case InputKeyLeft:
                g->held_left = down;
                break;
            case InputKeyRight:
                g->held_right = down;
                break;
            case InputKeyUp:
                g->held_up = down;
                break;
            case InputKeyDown:
                g->held_down = down;
                break;
            case InputKeyOk:
                g->held_ok = down;
                break;
            default:
                break;
            }
        }
        if(key == InputKeyBack && type == InputTypeShort) {
            release_all(g);
            g->state = StatePaused;
        }
        break;

    case StatePaused:
        if(key == InputKeyOk && type == InputTypeShort) g->state = StatePlaying;
        if(key == InputKeyBack && type == InputTypeShort) {
            record_high_score(g);
            g->state = StateTitle;
        }
        break;

    case StateGameOver:
        if(key == InputKeyOk && type == InputTypeShort) game_start(g);
        if(key == InputKeyBack && type == InputTypeShort) g->state = StateTitle;
        break;
    }
    return true;
}
