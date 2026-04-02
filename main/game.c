#include "game.h"
#include "input.h"
#include "font.h"
#include "sound.h"
#include "collision.h"
#include "meshes.h"
#include <string.h>  // memset

// ── Game init ─────────────────────────────────────────────────────────────────
void game_init(game_t *g)
{
    memset(g, 0, sizeof(*g));
    g->lives = 3;
    game_switch_scene(g, &SCENE_TITLE);
}

void game_switch_scene(game_t *g, const scene_t *next)
{
    scene_exit(g->current_scene, g);
    g->current_scene = next;
    scene_enter(g->current_scene, g);
}

// ── Entity management ─────────────────────────────────────────────────────────
entity_t *entity_spawn(game_t *g, const mesh_t *mesh, uint8_t type)
{
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (!g->entities[i].active) {
            entity_t *e = &g->entities[i];
            memset(e, 0, sizeof(*e));
            e->mesh            = mesh;
            e->type            = type;
            e->active          = true;
            e->transform.z     = 180; // sensible default depth
            e->fz              = INT_FP(180);
            e->collision_radius = INT_FP(20); // sensible default radius
            return e;
        }
    }
    return NULL; // no free slots
}

void entity_kill(entity_t *e)
{
    if (e) e->active = false;
}

void entity_update_positions(game_t *g)
{
    for (int i = 0; i < MAX_ENTITIES; i++) {
        entity_t *e = &g->entities[i];
        if (!e->active) continue;
        e->fx += e->vx;
        e->fy += e->vy;
        e->fz += e->vz;
        // Sync integer transform used by the renderer.
        e->transform.x = (int16_t)FP_INT(e->fx);
        e->transform.y = (int16_t)FP_INT(e->fy);
        e->transform.z = (int16_t)FP_INT(e->fz);
        // Keep depth positive to avoid divide-by-zero in projection.
        if (e->transform.z < 10) e->transform.z = 10;
    }
}

void entity_draw_all(game_t *g, uint8_t fb[8][128])
{
    if (g->camera_active)
        engine3d_set_camera(&g->camera);
    else
        engine3d_set_camera(NULL);

    for (int i = 0; i < MAX_ENTITIES; i++) {
        entity_t *e = &g->entities[i];
        if (!e->active || !e->mesh) continue;
        engine3d_draw_mesh(fb, e->mesh, &e->transform);
    }
}

// ── Scene: TITLE ──────────────────────────────────────────────────────────────
static uint8_t s_title_angle;

static void title_enter(game_t *g)
{
    s_title_angle = 0;
    // Spawn a decorative rotating cube.
    entity_t *cube = entity_spawn(g, &MESH_CUBE, ETYPE_NONE);
    if (cube) {
        cube->transform.x = 40;
        cube->transform.y = -10;
        cube->transform.z = 180;
        cube->fx = INT_FP(40);
        cube->fy = INT_FP(-10);
        cube->fz = INT_FP(180);
    }
}

static void title_exit(game_t *g)
{
    // Clear all entities before switching scenes.
    memset(g->entities, 0, sizeof(g->entities));
}

static void title_update(game_t *g, fp_t dt)
{
    (void)dt;
    // GPIO2 = BTN_LEFT: hold to freeze the title cube rotation.
    if (!input_held(BTN_LEFT))
        s_title_angle += 2;
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (g->entities[i].active)
            g->entities[i].transform.angle = s_title_angle;
    }
    if (input_just_pressed(BTN_ACTION)) {
        g->score = 0;
        g->lives = 3;
        game_switch_scene(g, &SCENE_GAMEPLAY);
    }
}

static void title_render(game_t *g, uint8_t fb[8][128])
{
    font_draw_string(fb,  2, 0, "3D ENGINE");
    font_draw_string(fb,  2, 8, "PRESS ACTION");
    entity_draw_all(g, fb);
}

const scene_t SCENE_TITLE = {
    .on_enter = title_enter,
    .on_exit  = title_exit,
    .update   = title_update,
    .render   = title_render,
};

// ── Scene: GAMEPLAY ───────────────────────────────────────────────────────────
// Minimal demo: one player ship (left/right move, ACTION spawns a bullet),
// one asteroid drifting in. Replace with your own game logic.

static entity_t *s_player;
static entity_t *s_asteroid;

static void gameplay_enter(game_t *g)
{
    s_player = entity_spawn(g, &MESH_SHIP, ETYPE_PLAYER);
    if (s_player) {
        s_player->fx              = INT_FP(0);
        s_player->fy              = INT_FP(10);
        s_player->fz              = INT_FP(160);
        s_player->collision_radius = INT_FP(15);
    }

    s_asteroid = entity_spawn(g, &MESH_ASTEROID, ETYPE_ENEMY);
    if (s_asteroid) {
        s_asteroid->fx  = INT_FP(30);
        s_asteroid->fy  = INT_FP(0);
        s_asteroid->fz  = INT_FP(60);
        s_asteroid->vz  = INT_FP(2); // drifts toward camera (+z = further, so this backs away)
        s_asteroid->collision_radius = INT_FP(20);
    }
}

static void gameplay_exit(game_t *g)
{
    memset(g->entities, 0, sizeof(g->entities));
    s_player   = NULL;
    s_asteroid = NULL;
}

static void on_hit(entity_t *a, entity_t *b, game_t *g)
{
    // Bullet hits asteroid.
    if ((a->type == ETYPE_BULLET && b->type == ETYPE_ENEMY) ||
        (a->type == ETYPE_ENEMY  && b->type == ETYPE_BULLET)) {
        entity_kill(a);
        entity_kill(b);
        g->score += 100;
        sound_play(SFX_EXPLODE);
        return;
    }
    // Player hits asteroid.
    if ((a->type == ETYPE_PLAYER && b->type == ETYPE_ENEMY) ||
        (a->type == ETYPE_ENEMY  && b->type == ETYPE_PLAYER)) {
        sound_play(SFX_EXPLODE);
        if (g->lives > 0) g->lives--;
        if (g->lives == 0)
            game_switch_scene(g, &SCENE_GAMEOVER);
    }
}

static void gameplay_update(game_t *g, fp_t dt)
{
    (void)dt;

    // Player movement.
    if (s_player && s_player->active) {
        if (input_held(BTN_LEFT))  s_player->fx -= INT_FP(3);
        if (input_held(BTN_RIGHT)) s_player->fx += INT_FP(3);
        if (input_held(BTN_UP))    s_player->fy -= INT_FP(2);
        if (input_held(BTN_DOWN))  s_player->fy += INT_FP(2);

        // Clamp to rough screen bounds (world coords at z=160).
        if (s_player->fx < INT_FP(-55)) s_player->fx = INT_FP(-55);
        if (s_player->fx >  INT_FP(55)) s_player->fx =  INT_FP(55);

        // Shoot.
        if (input_just_pressed(BTN_ACTION)) {
            entity_t *b = entity_spawn(g, &MESH_BULLET, ETYPE_BULLET);
            if (b) {
                b->fx = s_player->fx;
                b->fy = s_player->fy;
                b->fz = s_player->fz;
                b->vz = INT_FP(-5); // flies away from camera
                b->collision_radius = INT_FP(5);
            }
            sound_play(SFX_SHOOT);
        }
    }

    // Kill bullets that fly too far.
    for (int i = 0; i < MAX_ENTITIES; i++) {
        entity_t *e = &g->entities[i];
        if (e->active && e->type == ETYPE_BULLET && e->transform.z < 20)
            entity_kill(e);
    }

    entity_update_positions(g);
    collision_check_all(g, on_hit);

    // Rotate entities slightly each frame.
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (g->entities[i].active)
            g->entities[i].transform.angle += 2;
    }

    if (input_just_pressed(BTN_ACTION) && input_held(BTN_UP))
        game_switch_scene(g, &SCENE_PAUSE);
}

static void gameplay_render(game_t *g, uint8_t fb[8][128])
{
    entity_draw_all(g, fb);
    font_draw_string(fb, 0, 0, "SC:");
    font_draw_int(fb, 18, 0, g->score);
    font_draw_string(fb, 90, 0, "LV:");
    font_draw_int(fb, 108, 0, g->lives);
}

const scene_t SCENE_GAMEPLAY = {
    .on_enter = gameplay_enter,
    .on_exit  = gameplay_exit,
    .update   = gameplay_update,
    .render   = gameplay_render,
};

// ── Scene: PAUSE ──────────────────────────────────────────────────────────────
static void pause_update(game_t *g, fp_t dt)
{
    (void)dt;
    if (input_just_pressed(BTN_ACTION))
        game_switch_scene(g, &SCENE_GAMEPLAY);
}

static void pause_render(game_t *g, uint8_t fb[8][128])
{
    entity_draw_all(g, fb);
    font_draw_string(fb, 32, 28, "PAUSED");
    font_draw_string(fb, 10, 40, "ACTION TO RESUME");
}

const scene_t SCENE_PAUSE = {
    .on_enter = NULL,
    .on_exit  = NULL,
    .update   = pause_update,
    .render   = pause_render,
};

// ── Scene: GAME OVER ──────────────────────────────────────────────────────────
static void gameover_enter(game_t *g)
{
    (void)g;
    sound_play(SFX_GAMEOVER);
}

static void gameover_update(game_t *g, fp_t dt)
{
    (void)dt;
    if (input_just_pressed(BTN_ACTION)) {
        memset(g->entities, 0, sizeof(g->entities));
        g->score = 0;
        g->lives = 3;
        game_switch_scene(g, &SCENE_TITLE);
    }
}

static void gameover_render(game_t *g, uint8_t fb[8][128])
{
    font_draw_string(fb, 16, 20, "GAME OVER");
    font_draw_string(fb,  4, 32, "SCORE:");
    font_draw_int(fb, 40, 32, g->score);
    font_draw_string(fb,  4, 44, "ACTION TO RETRY");
}

const scene_t SCENE_GAMEOVER = {
    .on_enter = gameover_enter,
    .on_exit  = NULL,
    .update   = gameover_update,
    .render   = gameover_render,
};
