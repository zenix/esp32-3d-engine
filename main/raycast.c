#include "raycast.h"
#include "pattern.h"
#include "fixed_math.h"
#include <stdint.h>

// Must match engine3d.c constants.
#define FOCAL     150
#define CENTER_X   64
#define CENTER_Y   32
#define NEAR_PLANE  10

// Maximum DDA steps per ray.  A 32×32 map has diagonal = 45 tiles; 64 steps
// is generous enough while bounding worst-case runtime.
#define MAX_STEPS 64

// Minimum perpendicular distance (Q16.16) to avoid division by zero.
#define MIN_DIST  (1 << 8)   // 1/256 world unit

// Wall half-height numerator: half_screen_h * tile_sz = 32 * 64 = 2048.
// Change if tile_shift or screen height changes.
#define WALL_HALF_NUM 2048

// ── Pattern selection for each wall type and face side ────────────────────────
// N/S face (side=0) uses the primary pattern; E/W face (side=1) uses a
// secondary pattern that appears visually darker — simulating directional
// lighting without any runtime calculation.
static const uint8_t wall_pat[2][5] = {
    // side=0 (N/S, "bright" face)
    { PAT_SOLID,   PAT_SOLID,   PAT_HSTRIPE, PAT_DENSE, PAT_DIAG_L },
    // side=1 (E/W, "dark" face)
    { PAT_CHECK,   PAT_CHECK,   PAT_SPARSE,  PAT_CHECK, PAT_DIAG_R },
};

// ── Depth buffer ──────────────────────────────────────────────────────────────
static ray_hit_t s_hits[128];

const ray_hit_t *raycast_get_hits(void) { return s_hits; }

// ── raycast_render ────────────────────────────────────────────────────────────

void raycast_render(uint8_t fb[8][128], const raycast_map_t *map,
                    fp_t pos_x, fp_t pos_z, uint8_t yaw)
{
    // Precompute sin/cos once per frame (LUT lookups).
    fp_t sy = fp_sin(yaw);
    fp_t cy = fp_cos(yaw);

    // Forward direction  fwd = (-sin, cos) in (X, Z).
    // Right direction    rgt = ( cos, sin) in (X, Z).
    // Ray direction for column c:
    //   rd = fwd * FOCAL + rgt * (c - 64)
    // Precompute the FOCAL-scaled forward components once.
    fp_t fwd_x = FP_MUL(-sy, INT_FP(FOCAL));
    fp_t fwd_z = FP_MUL( cy, INT_FP(FOCAL));

    const int tile_sz  = 1 << map->tile_shift;
    const int px_int   = FP_INT(pos_x);
    const int pz_int   = FP_INT(pos_z);

    for (int col = 0; col < 128; col++) {
        // ── Ray direction ─────────────────────────────────────────────────────
        int offset = col - CENTER_X;   // -64 … +63
        fp_t rdx = fwd_x + FP_MUL(cy, INT_FP(offset));
        fp_t rdz = fwd_z + FP_MUL(sy, INT_FP(offset));

        // ── DDA setup ─────────────────────────────────────────────────────────
        int map_x = px_int >> map->tile_shift;
        int map_z = pz_int >> map->tile_shift;

        int step_x = (rdx >= 0) ? 1 : -1;
        int step_z = (rdz >= 0) ? 1 : -1;

        fp_t abs_rdx = rdx >= 0 ? rdx : -rdx;
        fp_t abs_rdz = rdz >= 0 ? rdz : -rdz;

        // dt: Q16.16 "time" to traverse one full tile in X or Z.
        // Guard against near-zero (ray almost perpendicular to axis).
        fp_t dt_x = (abs_rdx > 256) ? FP_DIV(INT_FP(tile_sz), abs_rdx) : 0x7FFFFFFF;
        fp_t dt_z = (abs_rdz > 256) ? FP_DIV(INT_FP(tile_sz), abs_rdz) : 0x7FFFFFFF;

        // Distance (in integer world units) from player to the first grid line
        // in each direction.
        int sub_x = px_int - (map_x << map->tile_shift);
        int sub_z = pz_int - (map_z << map->tile_shift);
        int dx0 = (step_x > 0) ? (tile_sz - sub_x) : sub_x;
        int dz0 = (step_z > 0) ? (tile_sz - sub_z) : sub_z;

        fp_t t_x = (abs_rdx > 256) ? FP_DIV(INT_FP(dx0), abs_rdx) : 0x7FFFFFFF;
        fp_t t_z = (abs_rdz > 256) ? FP_DIV(INT_FP(dz0), abs_rdz) : 0x7FFFFFFF;

        // ── DDA traversal ─────────────────────────────────────────────────────
        int side = 0;
        uint8_t cell = MAP_EMPTY;

        for (int step = 0; step < MAX_STEPS; step++) {
            if (t_x < t_z) {
                t_x  += dt_x;
                map_x += step_x;
                side   = 0;
            } else {
                t_z  += dt_z;
                map_z += step_z;
                side   = 1;
            }
            if ((unsigned)map_x >= map->width || (unsigned)map_z >= map->height)
                break;
            cell = map->cells[map_z * map->width + map_x];
            if (cell != MAP_EMPTY) break;
        }

        if (cell == MAP_EMPTY) {
            s_hits[col] = (ray_hit_t){ .dist = 0x7FFFFFFF };
            continue;
        }

        // ── Perpendicular distance ────────────────────────────────────────────
        // t_hit is the ray parameter just BEFORE the last DDA step advanced us
        // into the wall.  Multiplying by FOCAL gives perpendicular world-unit
        // distance (forward component of rd = FOCAL for any column — proved by
        // the dot-product identity sin²+cos²=1).
        fp_t t_hit = (side == 0) ? (t_x - dt_x) : (t_z - dt_z);
        fp_t perp  = FP_MUL(t_hit, INT_FP(FOCAL));
        if (perp < MIN_DIST) perp = MIN_DIST;

        s_hits[col].dist      = perp;
        s_hits[col].wall_type = cell;
        s_hits[col].side      = (uint8_t)side;

        // ── Wall strip height ─────────────────────────────────────────────────
        // half_h = (SCREEN_H/2) * tile_sz / perp  =  32 * 64 / perp
        int half_h = FP_INT(FP_DIV(INT_FP(WALL_HALF_NUM), perp));
        if (half_h > CENTER_Y) half_h = CENTER_Y;
        if (half_h < 1) {
            s_hits[col].dist = 0x7FFFFFFF;
            continue;
        }

        int y_top = CENTER_Y - half_h;
        int y_bot = CENTER_Y - 1 + half_h;

        // ── Pattern selection ─────────────────────────────────────────────────
        uint8_t wt = (cell <= MAP_WALL_MAX) ? cell : MAP_WALL_1;
        uint8_t pat = wall_pat[side][wt];

        pattern_vstrip(fb, col, y_top, y_bot, pat);
    }
}
