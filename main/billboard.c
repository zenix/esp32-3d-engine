#include "billboard.h"
#include "raycast.h"
#include "engine3d.h"   // engine3d_draw_pixel
#include "fixed_math.h"
#include <stdbool.h>

// Must match engine3d.c / raycast.c.
#define FOCAL     150
#define CENTER_X   64
#define CENTER_Y   32
#define NEAR_PLANE  10

// World half-height of a "standard" sprite unit (pixels scale from this).
// At distance = FOCAL the sprite appears at BASE_H pixels tall on screen.
#define SPR_BASE_H  48   // world units — enemies feel large and intimidating

// ── Sprite silhouettes ────────────────────────────────────────────────────────
// Each sprite is encoded as 8 columns.  For each column:
//   top[c] — topmost filled pixel, relative to centre; range -8..+8 (negative = up)
//   bot[c] — bottommost filled pixel, relative to centre; same range
// These are in "sprite units" where the full sprite height = BASE_H world units.
// The actual screen pixels are computed by scaling: screen_delta = unit * half_h / 8.

typedef struct { int8_t top[8]; int8_t bot[8]; } spr_def_t;

static const spr_def_t s_sprites[SPR_COUNT] = {
    // SPR_ENEMY_IDLE — oval body (skull), taller than wide
    {
        .top = { -3, -6, -8, -8, -8, -8, -6, -3 },
        .bot = {  5,  7,  8,  8,  8,  8,  7,  5 },
    },
    // SPR_ENEMY_ATCK — skull with arms raised (wider top)
    {
        .top = { -8, -8, -8, -8, -8, -8, -8, -8 },
        .bot = {  5,  7,  8,  8,  8,  8,  7,  5 },
    },
    // SPR_HEALTH — cross / plus: only centre columns and centre rows
    {
        .top = { -2, -2, -8, -8, -8, -8, -2, -2 },
        .bot = {  2,  2,  8,  8,  8,  8,  2,  2 },
    },
    // SPR_AMMO — wide flat rectangle
    {
        .top = { -2, -3, -3, -3, -3, -3, -3, -2 },
        .bot = {  2,  3,  3,  3,  3,  3,  3,  2 },
    },
    // SPR_KEY — tall narrow rectangle with a round bit at top
    {
        .top = { 0, -8, -8, -8, -8, -8, -8, 0 },
        .bot = { 0,  8,  4,  4,  4,  8,  8, 0 },
    },
};

// ── billboard_draw ────────────────────────────────────────────────────────────

bool billboard_draw(uint8_t fb[8][128],
                    fp_t wx, fp_t wz,
                    fp_t cam_x, fp_t cam_z, uint8_t cam_yaw,
                    uint8_t sprite_id)
{
    if (sprite_id >= SPR_COUNT) return false;

    // ── Transform world position to view space ────────────────────────────────
    fp_t dx = wx - cam_x;
    fp_t dz = wz - cam_z;

    fp_t cy = fp_cos(cam_yaw);
    fp_t sy = fp_sin(cam_yaw);

    // Camera view transform (same formula as engine3d.c lines 141-147):
    //   vx =  cos*dx + sin*dz
    //   vz = -sin*dx + cos*dz
    fp_t vx = FP_MUL(cy, dx) + FP_MUL(sy, dz);
    fp_t vz = FP_MUL(-sy, dx) + FP_MUL(cy, dz);

    if (FP_INT(vz) < NEAR_PLANE) return false;  // behind or at near plane

    // ── Project sprite centre to screen ───────────────────────────────────────
    int sx = FP_INT(FP_DIV(FP_MUL(vx, INT_FP(FOCAL)), vz)) + CENTER_X;

    // Half-height in screen pixels: scale BASE_H from world to screen.
    // half_h = BASE_H * FOCAL / (2 * vz)
    // To avoid overflow, compute as: half_h = FP_INT(FP_DIV(INT_FP(BASE_H * FOCAL / 2), vz))
    int half_h = FP_INT(FP_DIV(INT_FP(SPR_BASE_H * FOCAL / 2), vz));
    if (half_h < 1) return false;
    if (half_h > CENTER_Y) half_h = CENTER_Y;

    // Sprite aspect: width = 8 sprite columns mapped to screen width
    // screen_width = half_h * 2 * (8/16) = half_h  (8 columns / 16 units wide = 0.5 ratio)
    // Keep it simple: sprite screen width = half_h (square-ish at full size)
    int half_w = half_h / 2;
    if (half_w < 1) half_w = 1;

    int x_left  = sx - half_w;
    int x_right = sx + half_w - 1;

    // ── Per-column draw with depth clipping ───────────────────────────────────
    const ray_hit_t *hits = raycast_get_hits();
    const spr_def_t *spr  = &s_sprites[sprite_id];
    bool any_visible = false;

    for (int x = x_left; x <= x_right; x++) {
        if ((unsigned)x >= 128u) continue;

        // Depth test: skip columns where a wall is closer than the sprite.
        if (hits[x].dist <= vz) continue;

        // Map screen column to sprite column index (0-7).
        int spr_col = ((x - x_left) * 8) / (x_right - x_left + 1);
        if ((unsigned)spr_col >= 8u) spr_col = 7;

        // Compute screen y_top and y_bot for this sprite column.
        int y_top = CENTER_Y + (spr->top[spr_col] * half_h) / 8;
        int y_bot = CENTER_Y + (spr->bot[spr_col] * half_h) / 8;

        if (y_top > y_bot) continue;

        // Draw this column as a solid filled strip (sprite stands out against
        // patterned walls; solid fill is opaque and distinctive).
        // Use inverted pixels (draw pixel by pixel with alternating pattern)
        // to make the sprite pop even against solid walls.
        for (int y = y_top; y <= y_bot; y++) {
            if ((unsigned)y < 64u)
                engine3d_draw_pixel(fb, x, y);
        }
        any_visible = true;
    }

    return any_visible;
}
