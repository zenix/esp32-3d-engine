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

// ── Sprite bitmaps ────────────────────────────────────────────────────────────
// Each sprite: 16 columns × 16 rows.
//   cols[c]  — 16-bit column bitmap; bit r = row r (bit 0 = top, bit 15 = bottom)
//   1 = opaque pixel, 0 = transparent
//
// Rendering: for each screen column inside the sprite footprint the entire
// vertical span is cleared (AND-mask), then only the opaque bits are lit.
// This produces a black-background cutout that contrasts against any wall pattern.

typedef struct { uint16_t cols[16]; } spr_def_t;

static const spr_def_t s_sprites[SPR_COUNT] = {
    // SPR_ENEMY_IDLE — demon skull: hollow eyes, jagged teeth, neck
    //
    //  . . . . . . . . . . . . . . . .
    //  . . . . # # # # # # # # . . . .   row 1: head top
    //  . . . # # # # # # # # # # . . .   row 2
    //  . . # # . . # # # # . . # # . .   row 3: horn hints
    //  . # # . . . # # # # . . . # # .   row 4: eye sockets open
    //  . # # . . . # # # # . . . # # .   row 5
    //  . . # # # # # # # # # # # # . .   row 6: cheekbones
    //  . . # # # # # # # # # # # # . .   row 7
    //  . . # # # . . # # . . # # # . .   row 8: jagged teeth
    //  . . # . . . . . . . . . . # . .   row 9: mouth gap
    //  . . . # # # # # # # # # # . . .   row 10: chin
    //  . . . . # # # # # # # # . . . .   row 11
    //  . . . . . . # # # # . . . . . .   row 12: neck
    //  . . . . . . # # # # . . . . . .   row 13
    //  . . . . . . . . . . . . . . . .
    //  . . . . . . . . . . . . . . . .
    {{ 0x0000, 0x0030, 0x03F8, 0x05CC, 0x0DC6, 0x0CC6,
       0x3CFE, 0x3DFE, 0x3DFE, 0x3CFE, 0x0CC6, 0x0DC6,
       0x05CC, 0x03F8, 0x0030, 0x0000 }},

    // SPR_ENEMY_ATCK — arms raised wide, mouth wide open
    //
    //  # . . . . . . . . . . . . . . #   row 0: arm tips
    //  . # . . . . . . . . . . . . # .   row 1
    //  . . # . . . . . . . . . . # . .   row 2
    //  . . . # # # # # # # # # # . . .   row 3: head top
    //  . . . # . . # # # # . . # . . .   row 4: eye sockets
    //  . . . # . . # # # # . . # . . .   row 5
    //  . . . # # # # # # # # # # . . .   row 6: cheekbones
    //  . . . # # # # # # # # # # . . .   row 7
    //  . . . # # . . # # . . # # . . .   row 8: wide open mouth
    //  . . . # . . . . . . . . # . . .   row 9: bottom mouth
    //  . . . . # # # # # # # # . . . .   row 10: chin
    //  . . . . . # # # # # # . . . . .   row 11
    //  . . . . . . # # # # . . . . . .   row 12: neck
    //  . . . . . . # # # # . . . . . .   row 13
    //  . . . . . . . . . . . . . . . .
    //  . . . . . . . . . . . . . . . .
    {{ 0x0001, 0x0002, 0x0004, 0x03F8, 0x05C8, 0x0CC8,
       0x3CF8, 0x3DF8, 0x3DF8, 0x3CF8, 0x0CC8, 0x05C8,
       0x03F8, 0x0004, 0x0002, 0x0001 }},

    // SPR_HEALTH — plus / cross (vertical bar cols 6-9, horizontal bar cols 2-13)
    {{ 0x0000, 0x0000, 0x0180, 0x0180, 0x0180, 0x0180,
       0x0FF0, 0x0FF0, 0x0FF0, 0x0FF0, 0x0180, 0x0180,
       0x0180, 0x0180, 0x0000, 0x0000 }},

    // SPR_AMMO — outlined flat rectangle (cols 3-12, rows 6-9)
    {{ 0x0000, 0x0000, 0x0000, 0x03C0, 0x0240, 0x0240,
       0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240,
       0x03C0, 0x0000, 0x0000, 0x0000 }},

    // SPR_KEY — hollow ring top (cols 4-11) + shaft (cols 6-9) + tooth + cap
    //
    //  . . . . # # # # # # # # . . . .   row 1: ring top
    //  . . . # # . . . . . . # # . . .   row 2: ring sides
    //  . . . # . . . . . . . . # . . .   row 3
    //  . . . # . . . . . . . . # . . .   row 4
    //  . . . . # # # # # # # # . . . .   row 5: ring bottom
    //  . . . . . . # # # # . . . . . .   row 6: shaft
    //  . . . . . . # # # # . . . . . .   row 7
    //  . . . . . . # # # # . . . . . .   row 8
    //  . . . . . . # . . # . . . . . .   row 9: tooth gap
    //  . . . . . . # # # # . . . . . .   row 10
    //  . . . . . . # . . . . . . . . .   row 11: end cap
    {{ 0x0000, 0x0000, 0x0000, 0x001C, 0x0026, 0x0022,
       0x0FE2, 0x05E2, 0x05E2, 0x07E2, 0x0022, 0x0026,
       0x001C, 0x0000, 0x0000, 0x0000 }},
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

    // Camera view transform (same formula as engine3d.c):
    //   vx =  cos*dx + sin*dz
    //   vz = -sin*dx + cos*dz
    fp_t vx = FP_MUL(cy, dx) + FP_MUL(sy, dz);
    fp_t vz = FP_MUL(-sy, dx) + FP_MUL(cy, dz);

    if (FP_INT(vz) < NEAR_PLANE) return false;

    // ── Project sprite centre to screen ───────────────────────────────────────
    int sx = FP_INT(FP_DIV(FP_MUL(vx, INT_FP(FOCAL)), vz)) + CENTER_X;

    // half_h = BASE_H * FOCAL / (2 * vz)  — screen half-height in pixels
    int half_h = FP_INT(FP_DIV(INT_FP(SPR_BASE_H * FOCAL / 2), vz));
    if (half_h < 1) return false;
    if (half_h > CENTER_Y) half_h = CENTER_Y;

    // Sprite screen width = half_h (aspect ratio: sprite is twice as tall as wide)
    int half_w = half_h / 2;
    if (half_w < 1) half_w = 1;

    int x_left  = sx - half_w;
    int x_right = sx + half_w - 1;

    // ── Per-column draw with depth clipping ───────────────────────────────────
    const ray_hit_t *hits = raycast_get_hits();
    bool any_visible = false;

    int y_top = CENTER_Y - half_h;
    int y_bot = CENTER_Y + half_h;
    if (y_top < 0)  y_top = 0;
    if (y_bot > 63) y_bot = 63;
    int span = y_bot - y_top + 1;

    for (int x = x_left; x <= x_right; x++) {
        if ((unsigned)x >= 128u) continue;

        // Depth test: skip columns where a wall is closer than the sprite.
        if (hits[x].dist <= vz) continue;

        // Map screen column to source column (0-15).
        int spr_col = ((x - x_left) * 16) / (x_right - x_left + 1);
        if ((unsigned)spr_col >= 16u) spr_col = 15;

        uint16_t bitmap = s_sprites[sprite_id].cols[spr_col];
        if (bitmap == 0) continue;

        // Clear-then-draw: black out the sprite footprint, then set opaque pixels.
        // This creates a black background that contrasts against any wall pattern.
        for (int y = y_top; y <= y_bot; y++) {
            int row = ((y - y_top) * 16) / span;
            if (row > 15) row = 15;
            uint8_t bit = (uint8_t)(1u << (y & 7));
            if (bitmap & (1u << row))
                fb[y >> 3][x] |=  bit;
            else
                fb[y >> 3][x] &= ~bit;
        }
        any_visible = true;
    }

    return any_visible;
}
