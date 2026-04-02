#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "fixed_math.h"
#include "engine3d.h"
#include "scene.h"

// ── Entity types ──────────────────────────────────────────────────────────────
// Use any values you like in your game scenes.
#define ETYPE_NONE    0
#define ETYPE_PLAYER  1
#define ETYPE_ENEMY   2
#define ETYPE_BULLET  3
#define ETYPE_PICKUP  4

#define MAX_ENTITIES  16

// ── Entity ────────────────────────────────────────────────────────────────────
// Each active entity holds a mesh reference, precise position in Q16.16,
// integer velocity (applied each frame), and a sphere collision radius.
typedef struct entity_s {
    const mesh_t *mesh;

    // Integer screen/world position (used by engine3d_draw_mesh).
    // Derived from the precise fractional position below.
    transform_t transform;

    // Precise position in Q16.16. Update these; transform is synced automatically
    // by entity_update_positions().
    fp_t fx, fy, fz;

    // Velocity in Q16.16 units per frame (applied per entity_update_positions call).
    fp_t vx, vy, vz;

    // Sphere collision radius in Q16.16 units.
    fp_t collision_radius;

    // Application-defined type tag (ETYPE_* or your own).
    uint8_t type;

    bool active;
} entity_t;

// ── Game context ──────────────────────────────────────────────────────────────
// Make this static — it is too large for the main task stack.
typedef struct game_s {
    entity_t       entities[MAX_ENTITIES];
    const scene_t *current_scene;
    uint8_t        fb[8][128];   // 1 KB framebuffer
    int32_t        score;
    uint8_t        lives;
    uint16_t       frame_count;  // increments each frame, wraps at 65535
    camera_t       camera;
    bool           camera_active;
} game_t;

// ── Game lifecycle ────────────────────────────────────────────────────────────
void game_init(game_t *g);

// Transition to a new scene: calls on_exit on current, on_enter on next.
void game_switch_scene(game_t *g, const scene_t *next);

// ── Entity management ─────────────────────────────────────────────────────────

// Find a free slot, initialise it, return pointer. Returns NULL if all slots full.
entity_t *entity_spawn(game_t *g, const mesh_t *mesh, uint8_t type);

// Deactivate an entity (frees its slot for reuse).
void entity_kill(entity_t *e);

// Apply velocity to all active entities and sync transform to fractional position.
void entity_update_positions(game_t *g);

// Draw all active entities using the current camera state.
void entity_draw_all(game_t *g, uint8_t fb[8][128]);

// ── Built-in scenes ───────────────────────────────────────────────────────────
// Provided as starting points — override or replace in your own game.c files.
extern const scene_t SCENE_TITLE;
extern const scene_t SCENE_GAMEPLAY;
extern const scene_t SCENE_PAUSE;
extern const scene_t SCENE_GAMEOVER;
