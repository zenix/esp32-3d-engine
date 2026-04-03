#pragma once
#include <stdint.h>

// ── Fill Pattern Renderer ─────────────────────────────────────────────────────
// Provides dithered vertical-strip and rectangle fills for the SSD1306
// 128×64 monochrome framebuffer.  This is the "texture" system for a 1-bit
// display — different patterns distinguish wall types, doors, and surfaces.
//
// Patterns are tileable 8×8 bitmaps encoded as an 8-byte column LUT.
// Each byte encodes which of the 8 pixels in a vertical SSD1306 page column
// are lit (bit 0 = top of page, bit 7 = bottom of page).
//
// The column LUT repeats with period 8, matching the SSD1306 page height,
// so middle pages of a strip can be written with a single byte OR — very fast.

#define PAT_SOLID      0   // 100% — solid white
#define PAT_CHECK      1   //  50% — checkerboard (E/W wall shading in raycaster)
#define PAT_VSTRIPE    2   //  50% — vertical stripes (doors)
#define PAT_HSTRIPE    3   //  50% — horizontal stripes (alt wall material)
#define PAT_SPARSE     4   //  25% — sparse dots (dim / distant surfaces)
#define PAT_DENSE      5   //  75% — dense dots (alt wall type)
#define PAT_DIAG_L     6   //  25% — diagonal \ stripes (special walls)
#define PAT_DIAG_R     7   //  25% — diagonal / stripes (special walls)
#define PAT_COUNT      8

// Draw a vertical strip at column x, from y_top to y_bot inclusive,
// using the given fill pattern.  Bounds-checked; silently does nothing if
// x is out of range or y_top > y_bot.
void pattern_vstrip(uint8_t fb[8][128], int x, int y_top, int y_bot, uint8_t pat);

// Draw a filled rectangle with the given pattern.
void pattern_fill_rect(uint8_t fb[8][128], int x0, int y0, int w, int h, uint8_t pat);
