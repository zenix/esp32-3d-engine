#pragma once
#include <stdbool.h>

// Forward declarations — defined in game.h.
typedef struct entity_s entity_t;
typedef struct game_s   game_t;

// ── Sphere-sphere collision ───────────────────────────────────────────────────
// Compares squared distances using int64_t intermediates — no sqrt required.
// Uses the entity's precise fractional position (fx, fy, fz in Q16.16).
bool collision_check_spheres(const entity_t *a, const entity_t *b);

// ── Batch collision ───────────────────────────────────────────────────────────
// Checks all pairs of active entities (O(n²/2), max 120 pairs at n=16).
// Invokes the callback for every overlapping pair. a->type < b->type is NOT
// guaranteed; check both orderings in the callback if needed.
typedef void (*collision_cb_t)(entity_t *a, entity_t *b, game_t *g);
void collision_check_all(game_t *g, collision_cb_t cb);
