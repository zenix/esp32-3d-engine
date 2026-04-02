#include "asteroid.h"
#include "game.h"
#include "input.h"
#include "font.h"
#include "sound.h"
#include "particle.h"
#include "collision.h"
#include "meshes.h"
#include "esp_random.h"
#include <string.h>

// Coordinate conventions (confirmed by near-clip demo):
//   vz < 0  → z decreases → object moves toward camera (grows larger)
//   vz > 0  → z increases → object moves away from camera (shrinks)
//
// Layout:
//   Player ship: z=160 (fixed depth), moves in x-y plane
//   Asteroids:   spawn at z=340, vz=-2 (approach camera, grow larger)
//   Bullets:     spawn at player z=160, vz=+6 (fly into background, shrink)
//   Diamonds:    spawn at z=340, vz=-1 (slow approach, bonus pickup)
//   Cull asteroids/diamonds at z < 100 (past player): costs 1 life

#define PLAYER_Z       160
#define SPAWN_Z_MIN    320
#define SPAWN_Z_MAX    360
#define CULL_Z          100   // asteroid passed player
#define BULLET_CULL_Z  350   // bullet flew out of range

// ── Static gameplay state ─────────────────────────────────────────────────────
static entity_t *s_player;
static uint8_t   s_spawn_timer;
static uint8_t   s_spawn_interval;
static uint8_t   s_diamond_timer;
static uint8_t   s_invincible;    // frames of post-hit invincibility
static bool      s_to_gameover;  // deferred scene switch flag

// ── Helper: random int in [lo, hi] ────────────────────────────────────────────
static int rng_range(int lo, int hi)
{
    return lo + (int)(esp_random() % (unsigned)(hi - lo + 1));
}

// ── Helper: difficulty — recalculate spawn interval from score ────────────────
static void update_difficulty(int32_t score)
{
    int32_t iv = 50 - (score / 500) * 5;
    if (iv < 15) iv = 15;
    s_spawn_interval = (uint8_t)iv;
}

// ── Helper: spawn one asteroid ────────────────────────────────────────────────
static void spawn_asteroid(game_t *g)
{
    entity_t *a = entity_spawn(g, &MESH_ASTEROID, ETYPE_ENEMY);
    if (!a) return;
    a->fx = INT_FP(rng_range(-50, 50));
    a->fy = INT_FP(rng_range(-15, 15));
    a->fz = INT_FP(rng_range(SPAWN_Z_MIN, SPAWN_Z_MAX));
    a->vz = -INT_FP(2);
    a->transform.axis   = AXIS_DEFAULT;
    a->transform.scale  = 128;
    a->collision_radius = INT_FP(20);
}

// ── Helper: spawn one diamond ─────────────────────────────────────────────────
static void spawn_diamond(game_t *g)
{
    entity_t *d = entity_spawn(g, &MESH_DIAMOND, ETYPE_PICKUP);
    if (!d) return;
    d->fx = INT_FP(rng_range(-40, 40));
    d->fy = INT_FP(rng_range(-10, 10));
    d->fz = INT_FP(rng_range(SPAWN_Z_MIN, SPAWN_Z_MAX));
    d->vz = -INT_FP(1);
    d->transform.axis   = AXIS_Y;
    d->transform.scale  = 128;
    d->collision_radius = INT_FP(12);
}

// ── Collision callback ────────────────────────────────────────────────────────
static void on_hit(entity_t *a, entity_t *b, game_t *g)
{
    entity_t *bullet = NULL, *enemy = NULL, *player = NULL, *pickup = NULL;
    if (a->type == ETYPE_BULLET) bullet = a;
    if (b->type == ETYPE_BULLET) bullet = b;
    if (a->type == ETYPE_ENEMY)  enemy  = a;
    if (b->type == ETYPE_ENEMY)  enemy  = b;
    if (a->type == ETYPE_PLAYER) player = a;
    if (b->type == ETYPE_PLAYER) player = b;
    if (a->type == ETYPE_PICKUP) pickup = a;
    if (b->type == ETYPE_PICKUP) pickup = b;

    if (bullet && enemy) {
        particle_spawn_burst(enemy->fx, enemy->fy, enemy->fz, 10);
        entity_kill(bullet);
        entity_kill(enemy);
        sound_play(SFX_EXPLODE);
        g->score += 100;
        update_difficulty(g->score);
        return;
    }

    if (player && enemy && s_invincible == 0) {
        particle_spawn_burst(enemy->fx, enemy->fy, enemy->fz, 8);
        entity_kill(enemy);
        sound_play(SFX_EXPLODE);
        s_invincible = 45;
        if (g->lives > 0) g->lives--;
        if (g->lives == 0) s_to_gameover = true;
        return;
    }

    if (player && pickup) {
        entity_kill(pickup);
        sound_play(SFX_PICKUP);
        g->score += 250;
    }
}

// ── SCENE_ASTEROID_TITLE ──────────────────────────────────────────────────────
static void title_enter(game_t *g)
{
    for (int i = 0; i < MAX_ENTITIES; i++) entity_kill(&g->entities[i]);
    g->camera_active = false;

    entity_t *cube = entity_spawn(g, &MESH_CUBE_CULLED, ETYPE_NONE);
    if (cube) {
        cube->fx = INT_FP(36);
        cube->fy = INT_FP(-5);
        cube->fz = INT_FP(180);
        cube->transform.axis  = AXIS_Y;
        cube->transform.scale = 128;
    }
}

static void title_exit(game_t *g)
{
    for (int i = 0; i < MAX_ENTITIES; i++) entity_kill(&g->entities[i]);
}

static void title_update(game_t *g, fp_t dt)
{
    (void)dt;
    for (int i = 0; i < MAX_ENTITIES; i++)
        if (g->entities[i].active) g->entities[i].transform.angle += 2;

    if (input_just_pressed(BTN_ACTION)) {
        g->score = 0;
        g->lives = 3;
        game_switch_scene(g, &SCENE_ASTEROID_PLAY);
    }
}

static void title_render(game_t *g, uint8_t fb[8][128])
{
    entity_draw_all(g, fb);
    font_draw_string(fb,  2,  0, "ASTEROID");
    font_draw_string(fb,  2,  8, "BLASTER");
    font_draw_string(fb,  2, 48, "PRESS ACTION");
}

const scene_t SCENE_ASTEROID_TITLE = {
    .on_enter = title_enter,
    .on_exit  = title_exit,
    .update   = title_update,
    .render   = title_render,
};

// ── SCENE_ASTEROID_PLAY ───────────────────────────────────────────────────────
static void play_enter(game_t *g)
{
    for (int i = 0; i < MAX_ENTITIES; i++) entity_kill(&g->entities[i]);
    g->camera_active = false;
    s_to_gameover    = false;
    s_invincible     = 0;
    s_spawn_interval = 50;
    s_spawn_timer    = 50;
    s_diamond_timer  = 150;

    s_player = entity_spawn(g, &MESH_SHIP, ETYPE_PLAYER);
    if (s_player) {
        s_player->fx = 0;
        s_player->fy = INT_FP(5);
        s_player->fz = INT_FP(PLAYER_Z);
        s_player->transform.axis   = AXIS_X;
        s_player->transform.angle  = 224; // 315° cancels the Rx(45°) engine pre-tilt
        s_player->transform.scale  = 128;
        s_player->collision_radius = INT_FP(15);
    }
}

static void play_exit(game_t *g)
{
    for (int i = 0; i < MAX_ENTITIES; i++) entity_kill(&g->entities[i]);
    s_player = NULL;
}

static void play_update(game_t *g, fp_t dt)
{
    (void)dt;

    if (s_invincible > 0) s_invincible--;

    // Player movement
    if (s_player && s_player->active) {
        if (input_held(BTN_LEFT))  s_player->fx -= INT_FP(3);
        if (input_held(BTN_RIGHT)) s_player->fx += INT_FP(3);
        if (input_held(BTN_UP))    s_player->fy -= INT_FP(2);
        if (input_held(BTN_DOWN))  s_player->fy += INT_FP(2);
        if (s_player->fx < INT_FP(-50)) s_player->fx = INT_FP(-50);
        if (s_player->fx >  INT_FP(50)) s_player->fx =  INT_FP(50);
        if (s_player->fy < INT_FP(-20)) s_player->fy = INT_FP(-20);
        if (s_player->fy >  INT_FP(20)) s_player->fy =  INT_FP(20);

        // Fire bullet
        if (input_just_pressed(BTN_ACTION)) {
            entity_t *b = entity_spawn(g, &MESH_BULLET, ETYPE_BULLET);
            if (b) {
                b->fx = s_player->fx;
                b->fy = s_player->fy;
                b->fz = s_player->fz;
                b->vz = INT_FP(6);   // flies into background
                b->transform.axis   = AXIS_Z;
                b->transform.scale  = 64;
                b->collision_radius = INT_FP(5);
            }
            sound_play(SFX_SHOOT);
        }
    }

    // Spawn timers
    if (--s_spawn_timer == 0) {
        s_spawn_timer = s_spawn_interval;
        spawn_asteroid(g);
    }
    if (--s_diamond_timer == 0) {
        s_diamond_timer = 180;
        spawn_diamond(g);
    }

    // Cull out-of-range entities
    for (int i = 0; i < MAX_ENTITIES; i++) {
        entity_t *e = &g->entities[i];
        if (!e->active) continue;
        if (e->type == ETYPE_BULLET && e->transform.z > BULLET_CULL_Z) {
            entity_kill(e);
            continue;
        }
        if ((e->type == ETYPE_ENEMY || e->type == ETYPE_PICKUP)
                && e->transform.z < CULL_Z) {
            if (e->type == ETYPE_ENEMY && s_invincible == 0) {
                sound_play(SFX_EXPLODE);
                s_invincible = 45;
                if (g->lives > 0) g->lives--;
                if (g->lives == 0) { entity_kill(e); s_to_gameover = true; break; }
            }
            entity_kill(e);
        }
    }

    entity_update_positions(g);

    // Spin asteroids and diamonds; keep player ship at a fixed angle
    for (int i = 0; i < MAX_ENTITIES; i++)
        if (g->entities[i].active && g->entities[i].type != ETYPE_PLAYER)
            g->entities[i].transform.angle += 2;

    collision_check_all(g, on_hit);

    // Deferred scene switch (never call game_switch_scene mid-collision-loop)
    if (s_to_gameover)
        game_switch_scene(g, &SCENE_ASTEROID_OVER);
}

static void play_render(game_t *g, uint8_t fb[8][128])
{
    entity_draw_all(g, fb);
    font_draw_string(fb,  0, 0, "SC:");
    font_draw_int(fb,    18, 0, g->score);
    font_draw_string(fb, 90, 0, "LV:");
    font_draw_int(fb,   108, 0, g->lives);
    // Flash "!!" indicator during invincibility
    if (s_invincible > 0 && (s_invincible & 4))
        font_draw_string(fb, 50, 0, "!!");
}

const scene_t SCENE_ASTEROID_PLAY = {
    .on_enter = play_enter,
    .on_exit  = play_exit,
    .update   = play_update,
    .render   = play_render,
};

// ── SCENE_ASTEROID_OVER ───────────────────────────────────────────────────────
static void over_enter(game_t *g)
{
    (void)g;
    sound_play(SFX_GAMEOVER);
}

static void over_update(game_t *g, fp_t dt)
{
    (void)dt;
    if (input_just_pressed(BTN_ACTION))
        game_switch_scene(g, &SCENE_ASTEROID_TITLE);
}

static void over_render(game_t *g, uint8_t fb[8][128])
{
    font_draw_string(fb, 16, 16, "GAME OVER");
    font_draw_string(fb,  4, 28, "SCORE:");
    font_draw_int(fb,    40, 28, g->score);
    font_draw_string(fb,  4, 40, "ACTION: RETRY");
}

const scene_t SCENE_ASTEROID_OVER = {
    .on_enter = over_enter,
    .on_exit  = NULL,
    .update   = over_update,
    .render   = over_render,
};
