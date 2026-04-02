#pragma once
#include <stdint.h>
#include "fixed_math.h"

// ── Scene / State Machine ─────────────────────────────────────────────────────
// A scene owns one game state (title, gameplay, pause, game-over, etc.).
// The main loop calls scene_update() then scene_render() each frame.
// Use game_switch_scene() (in game.h) to transition between scenes.

// Forward declaration — game_t is defined in game.h.
typedef struct game_s game_t;

typedef struct {
    // Called once when this scene becomes current (initialise state here).
    void (*on_enter)(game_t *g);
    // Called once when leaving this scene (clean up here).
    void (*on_exit)(game_t *g);
    // Called every frame with the fixed delta-time (Q16.16 seconds).
    void (*update)(game_t *g, fp_t dt);
    // Called every frame to fill the framebuffer; flush happens after.
    void (*render)(game_t *g, uint8_t fb[8][128]);
} scene_t;

// NULL-safe helpers — call these instead of raw function pointers.
static inline void scene_update(const scene_t *s, game_t *g, fp_t dt)
{
    if (s && s->update) s->update(g, dt);
}
static inline void scene_render(const scene_t *s, game_t *g, uint8_t fb[8][128])
{
    if (s && s->render) s->render(g, fb);
}
static inline void scene_enter(const scene_t *s, game_t *g)
{
    if (s && s->on_enter) s->on_enter(g);
}
static inline void scene_exit(const scene_t *s, game_t *g)
{
    if (s && s->on_exit) s->on_exit(g);
}
