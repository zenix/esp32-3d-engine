#pragma once
#include <stdint.h>
#include "fixed_math.h"

// ── DDA Raycaster ─────────────────────────────────────────────────────────────
// 2.5D raycasting engine (Wolfenstein-style) for the 128×64 SSD1306 display.
// Casts one ray per screen column (128 total) through a 2D tile grid and
// renders wall strips using the pattern fill system.
//
// Coordinate system: XZ plane (Y is up and unused by the raycaster).
//   - At yaw=0  the player faces +Z.
//   - At yaw=64 the player faces -X.
// This matches the existing engine3d camera convention.
//
// Tile cell values:
//   0            — empty (passable)
//   MAP_WALL_1…7 — various wall types, each with a different fill pattern
// Any non-zero value is treated as solid by the DDA traversal.
//
// Call raycast_render() once per frame; then call raycast_get_hits() to
// retrieve the 128-column depth buffer for sprite occlusion tests.

// ── Map cell constants ────────────────────────────────────────────────────────
#define MAP_EMPTY    0
#define MAP_WALL_1   1   // N/S: solid,   E/W: checkerboard
#define MAP_WALL_2   2   // N/S: hstripe, E/W: sparse
#define MAP_WALL_3   3   // N/S: dense,   E/W: checkerboard
#define MAP_WALL_4   4   // N/S: diag-L,  E/W: diag-R
#define MAP_WALL_MAX 4

// ── Map descriptor ────────────────────────────────────────────────────────────
// cells[] may be mutable so the game can open/close doors by writing 0 or
// the original wall type into the grid.
typedef struct {
    uint8_t *cells;     // row-major: cells[z * width + x]; 0 = empty
    uint8_t  width;
    uint8_t  height;
    uint8_t  tile_shift;  // log2(tile_size), e.g. 6 for 64-unit tiles
} raycast_map_t;

// ── Per-column ray result ─────────────────────────────────────────────────────
// Stored in a static 128-entry buffer and accessible via raycast_get_hits().
// The dist field is in Q16.16 world units; use it to depth-clip sprites.
typedef struct {
    fp_t    dist;       // perpendicular wall distance (Q16.16 world units)
    uint8_t wall_type;  // MAP_WALL_* value of the cell hit (0 if no hit)
    uint8_t side;       // 0 = hit X-boundary (N/S face)  1 = Z-boundary (E/W)
} ray_hit_t;

// ── API ───────────────────────────────────────────────────────────────────────

// Cast all 128 rays and render wall strips into fb.
//   pos_x, pos_z — player world position in Q16.16.
//   yaw          — player facing angle (0-255 = 0-360°, same as engine3d).
// The depth buffer (raycast_get_hits) is updated on each call.
void raycast_render(uint8_t fb[8][128], const raycast_map_t *map,
                    fp_t pos_x, fp_t pos_z, uint8_t yaw);

// Return pointer to the 128-entry ray_hit_t buffer from the last
// raycast_render() call.  Valid until the next raycast_render() call.
const ray_hit_t *raycast_get_hits(void);
