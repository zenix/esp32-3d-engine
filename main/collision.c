#include "collision.h"
#include "game.h"
#include "fixed_math.h"

bool collision_check_spheres(const entity_t *a, const entity_t *b)
{
    // Positions are Q16.16.  Compute squared distance with int64_t intermediates
    // to prevent int32_t overflow (each term can reach ~6e14 before shift).
    fp_t dx = a->fx - b->fx;
    fp_t dy = a->fy - b->fy;
    fp_t dz = a->fz - b->fz;

    // Each product is Q32.32; shift back to Q16.16 before summing.
    int64_t dist2 = (((int64_t)dx * dx) >> FP_SHIFT)
                  + (((int64_t)dy * dy) >> FP_SHIFT)
                  + (((int64_t)dz * dz) >> FP_SHIFT);

    fp_t r_sum = a->collision_radius + b->collision_radius;
    int64_t r2 = ((int64_t)r_sum * r_sum) >> FP_SHIFT;

    return dist2 <= r2;
}

void collision_check_all(game_t *g, collision_cb_t cb)
{
    for (int i = 0; i < MAX_ENTITIES - 1; i++) {
        if (!g->entities[i].active) continue;
        for (int j = i + 1; j < MAX_ENTITIES; j++) {
            if (!g->entities[j].active) continue;
            if (collision_check_spheres(&g->entities[i], &g->entities[j]))
                cb(&g->entities[i], &g->entities[j], g);
        }
    }
}
