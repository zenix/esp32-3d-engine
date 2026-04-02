#include "demo.h"
#include "game.h"
#include "input.h"
#include "font.h"
#include "sound.h"
#include "particle.h"
#include "collision.h"
#include "meshes.h"
#include <string.h>

#define NUM_PAGES   6
#define MESH_HOLD  60   // frames each mesh is shown on page 0 (~2 s at 30 fps)

static uint8_t s_page;
static uint8_t s_mesh_idx;   // page 0: which mesh is shown (0-4)
static uint8_t s_mesh_timer; // page 0: countdown to next mesh

static const mesh_t *const s_p0_meshes[5] = {
    &MESH_CUBE, &MESH_SHIP, &MESH_ASTEROID, &MESH_BULLET, &MESH_DIAMOND,
};
static const char *const s_p0_names[5] = {
    "CUBE", "SHIP", "ASTEROID", "BULLET", "DIAMOND",
};

// ── Helpers ───────────────────────────────────────────────────────────────────
static entity_t *spawn(game_t *g, const mesh_t *m, uint8_t type,
                        int16_t x, int16_t z, uint8_t axis, uint8_t scale)
{
    entity_t *e = entity_spawn(g, m, type);
    if (!e) return NULL;
    e->fx = INT_FP(x); e->fy = 0; e->fz = INT_FP(z);
    e->transform.axis  = axis;
    e->transform.scale = scale;
    return e;
}

static void kill_all(game_t *g)
{
    for (int i = 0; i < MAX_ENTITIES; i++)
        entity_kill(&g->entities[i]);
    g->camera_active = false;
}

static void spin_all(game_t *g, uint8_t step)
{
    for (int i = 0; i < MAX_ENTITIES; i++)
        if (g->entities[i].active) g->entities[i].transform.angle += step;
}

// ── Page enter ────────────────────────────────────────────────────────────────
static void enter_page(game_t *g)
{
    kill_all(g);
    g->score = 0;

    switch (s_page) {
    case 0:
        s_mesh_idx   = 0;
        s_mesh_timer = MESH_HOLD;
        spawn(g, s_p0_meshes[0], ETYPE_NONE, 0, 180, AXIS_DEFAULT, 128);
        break;

    case 1:
        s_mesh_idx   = 0;
        s_mesh_timer = MESH_HOLD;
        spawn(g, &MESH_CUBE, ETYPE_NONE, 0, 180, AXIS_X, 64);
        break;

    case 2:
        s_mesh_idx   = 0;
        s_mesh_timer = MESH_HOLD;
        spawn(g, &MESH_CUBE_CULLED, ETYPE_NONE, 0, 180, AXIS_DEFAULT, 128);
        break;

    case 3: {
        entity_t *e = spawn(g, &MESH_DIAMOND, ETYPE_NONE, 0, 200, AXIS_DEFAULT, 128);
        if (e) e->vz = INT_FP(-3);
        break;
    }

    case 4:
        spawn(g, &MESH_CUBE,    ETYPE_NONE, -10, 150, AXIS_DEFAULT, 128);
        spawn(g, &MESH_DIAMOND, ETYPE_NONE,  10, 120, AXIS_DEFAULT, 128);
        g->camera.x = 0; g->camera.y = 0; g->camera.z = 0; g->camera.yaw = 0;
        g->camera_active = true;
        break;

    case 5: {
        entity_t *p = spawn(g, &MESH_SHIP,     ETYPE_PLAYER,  0, 160, AXIS_DEFAULT, 128);
        entity_t *a = spawn(g, &MESH_ASTEROID, ETYPE_ENEMY,  20,  60, AXIS_DEFAULT, 128);
        if (p) p->collision_radius = INT_FP(15);
        if (a) { a->vz = INT_FP(2); a->collision_radius = INT_FP(20); }
        break;
    }
    }
}

// ── Page 5 collision callback ─────────────────────────────────────────────────
static void on_hit(entity_t *a, entity_t *b, game_t *g)
{
    if (!((a->type == ETYPE_PLAYER || b->type == ETYPE_PLAYER) &&
          (a->type == ETYPE_ENEMY  || b->type == ETYPE_ENEMY))) return;

    entity_t *enemy = (a->type == ETYPE_ENEMY) ? a : b;
    particle_spawn_burst(enemy->fx, enemy->fy, enemy->fz, 12);
    entity_kill(a);
    entity_kill(b);
    sound_play(SFX_EXPLODE);
    g->score++;

    entity_t *na = entity_spawn(g, &MESH_ASTEROID, ETYPE_ENEMY);
    if (na) {
        na->fx = INT_FP(20); na->fy = 0; na->fz = INT_FP(60);
        na->vz = INT_FP(2);
        na->collision_radius = INT_FP(20);
    }
}

// ── Per-page update ───────────────────────────────────────────────────────────
static void update_page(game_t *g, fp_t dt)
{
    (void)dt;
    switch (s_page) {

    case 0:
        spin_all(g, 2);
        entity_update_positions(g);
        if (--s_mesh_timer == 0) {
            s_mesh_timer = MESH_HOLD;
            s_mesh_idx   = (s_mesh_idx + 1) % 5;
            kill_all(g);
            spawn(g, s_p0_meshes[s_mesh_idx], ETYPE_NONE, 0, 180, AXIS_DEFAULT, 128);
        }
        break;

    case 1: {
        static const uint8_t p1_axes[3]   = {AXIS_X, AXIS_Y, AXIS_Z};
        static const uint8_t p1_scales[3] = {64, 128, 192};
        spin_all(g, 2);
        entity_update_positions(g);
        if (--s_mesh_timer == 0) {
            s_mesh_timer = MESH_HOLD;
            s_mesh_idx   = (s_mesh_idx + 1) % 3;
            kill_all(g);
            spawn(g, &MESH_CUBE, ETYPE_NONE, 0, 180, p1_axes[s_mesh_idx], p1_scales[s_mesh_idx]);
        }
        break;
    }

    case 2:
        spin_all(g, 2);
        entity_update_positions(g);
        if (--s_mesh_timer == 0) {
            s_mesh_timer = MESH_HOLD;
            s_mesh_idx   = (s_mesh_idx + 1) % 2;
            kill_all(g);
            spawn(g, s_mesh_idx == 0 ? &MESH_CUBE_CULLED : &MESH_ASTEROID,
                  ETYPE_NONE, 0, 180, AXIS_DEFAULT, 128);
        }
        break;

    case 3:
        spin_all(g, 3);
        entity_update_positions(g);
        for (int i = 0; i < MAX_ENTITIES; i++) {
            entity_t *e = &g->entities[i];
            if (e->active && e->transform.z < 12) {
                e->fz = INT_FP(200); e->transform.z = 200;
            }
        }
        break;

    case 4:
        if (input_held(BTN_LEFT))  g->camera.yaw -= 2;
        if (input_held(BTN_RIGHT)) g->camera.yaw += 2;
        spin_all(g, 2);
        entity_update_positions(g);
        break;

    case 5: {
        for (int i = 0; i < MAX_ENTITIES; i++) {
            entity_t *e = &g->entities[i];
            if (!e->active || e->type != ETYPE_PLAYER) continue;
            if (input_held(BTN_LEFT))  e->fx -= INT_FP(3);
            if (input_held(BTN_RIGHT)) e->fx += INT_FP(3);
            if (e->fx < INT_FP(-55)) e->fx = INT_FP(-55);
            if (e->fx >  INT_FP(55)) e->fx =  INT_FP(55);
        }
        spin_all(g, 2);
        entity_update_positions(g);
        collision_check_all(g, on_hit);
        break;
    }
    }
}

// ── Per-page render ───────────────────────────────────────────────────────────
static void render_page(game_t *g, uint8_t fb[8][128])
{
    entity_draw_all(g, fb);

    switch (s_page) {
    case 0:
        font_draw_string(fb, 0, 56, s_p0_names[s_mesh_idx]);
        break;
    case 1: {
        static const char *p1_labels[3] = {"AXIS:X 0.5x", "AXIS:Y 1x", "AXIS:Z 1.5x"};
        font_draw_string(fb, 0, 56, p1_labels[s_mesh_idx]);
        break;
    }
    case 2:
        font_draw_string(fb, 0, 56, s_mesh_idx == 0 ? "CULL ON" : "NO CULL");
        break;
    case 3: {
        int16_t z = 0;
        for (int i = 0; i < MAX_ENTITIES; i++)
            if (g->entities[i].active) { z = g->entities[i].transform.z; break; }
        font_draw_string(fb, 0, 56, "NEAR CLIP Z=");
        font_draw_int(fb, 72, 56, z);
        break;
    }
    case 4:
        font_draw_string(fb, 0, 56, "L/R:YAW  DEPTH SORT");
        break;
    case 5:
        particle_draw(fb, 0, 0, 0);
        font_draw_string(fb, 0, 56, "L/R:MOVE  HITS:");
        font_draw_int(fb, 96, 56, g->score);
        break;
    }
}

// ── Scene callbacks ───────────────────────────────────────────────────────────
static void demo_enter(game_t *g)
{
    s_page = 0;
    sound_play(SFX_PICKUP);
    enter_page(g);
}

static void demo_exit(game_t *g) { kill_all(g); }

static void demo_update(game_t *g, fp_t dt)
{
    if (input_just_pressed(BTN_ACTION)) {
        s_page = (uint8_t)((s_page + 1) % NUM_PAGES);
        sound_play(SFX_PICKUP);
        enter_page(g);
        return;
    }
    update_page(g, dt);
}

static void demo_render(game_t *g, uint8_t fb[8][128])
{
    render_page(g, fb);
    font_draw_int(fb, 104, 0, s_page + 1);
    font_draw_char(fb, 110, 0, '/');
    font_draw_int(fb, 116, 0, NUM_PAGES);
}

const scene_t SCENE_DEMO = {
    .on_enter = demo_enter,
    .on_exit  = demo_exit,
    .update   = demo_update,
    .render   = demo_render,
};
