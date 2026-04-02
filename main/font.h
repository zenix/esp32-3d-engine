#pragma once
#include <stdint.h>

// ── 5×7 Bitmap Font ───────────────────────────────────────────────────────────
// Characters are 5 pixels wide × 7 pixels tall (column-major, bit 0 = top row).
// Rendered with 1-pixel horizontal gap → 6 pixels per character cell.
// Covers ASCII 32 (space) through 126 (~).
//
// Coordinate system: (x, y) = pixel position of the top-left of the character.
// y = 0 means the top row of the display; y = 57 is the last row a 7px-tall
// character fits without clipping (57 + 7 = 64).
//
// Page-aligned fast path: when (y & 7) == 0, glyph bytes map directly onto
// SSD1306 pages with no bit shifting. Optimal for y = 0, 8, 16, 24, 32, 40, 48.

// Draw a single character. Out-of-bounds x/y are clipped.
void font_draw_char(uint8_t fb[8][128], int x, int y, char ch);

// Draw a null-terminated string. Characters advance 6 pixels per glyph.
void font_draw_string(uint8_t fb[8][128], int x, int y, const char *str);

// Draw a signed decimal integer without using printf/sprintf.
// Handles values from INT32_MIN to INT32_MAX.
void font_draw_int(uint8_t fb[8][128], int x, int y, int32_t value);
