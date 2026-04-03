#include "pattern.h"

// ── Pattern LUT ───────────────────────────────────────────────────────────────
// pat_lut[pattern][x & 7] = byte mask for SSD1306 page column at screen x.
// Bit 0 = top pixel of page, bit 7 = bottom pixel of page.
// The period-8 repetition matches the SSD1306 page height exactly, so a strip
// spanning full pages can be drawn with a single byte OR per page (no shifting).

static const uint8_t pat_lut[PAT_COUNT][8] = {
    // PAT_SOLID   — all pixels lit
    {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},
    // PAT_CHECK   — checkerboard: (x+y) even → lit
    {0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA},
    // PAT_VSTRIPE — vertical stripes: even columns lit, odd columns dark
    {0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00},
    // PAT_HSTRIPE — horizontal stripes: even rows lit (bit 0,2,4,6 = 0x55)
    {0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55},
    // PAT_SPARSE  — sparse dots ~25%: every other row, every other column
    {0x55,0x00,0x55,0x00,0x55,0x00,0x55,0x00},
    // PAT_DENSE   — dense dots ~75%: inverse of sparse + solid cols
    {0xFF,0xAA,0xFF,0xAA,0xFF,0xAA,0xFF,0xAA},
    // PAT_DIAG_L  — diagonal \ stripes: rotate 0x11 left by column
    {0x11,0x22,0x44,0x88,0x11,0x22,0x44,0x88},
    // PAT_DIAG_R  — diagonal / stripes: rotate 0x88 right by column
    {0x88,0x44,0x22,0x11,0x88,0x44,0x22,0x11},
};

// ── pattern_vstrip ────────────────────────────────────────────────────────────

void pattern_vstrip(uint8_t fb[8][128], int x, int y_top, int y_bot, uint8_t pat)
{
    if ((unsigned)x >= 128u) return;
    if (y_top < 0)  y_top = 0;
    if (y_bot > 63) y_bot = 63;
    if (y_top > y_bot) return;

    uint8_t pbyte = pat_lut[pat & (PAT_COUNT - 1)][x & 7];
    if (!pbyte) return;  // empty column in this pattern — nothing to draw

    int page_top = y_top >> 3;
    int page_bot = y_bot >> 3;

    if (page_top == page_bot) {
        // Strip fits within a single SSD1306 page — combine both clip masks.
        // Top mask:    bits [y_top&7 .. 7]  =  0xFF << (y_top & 7)
        // Bottom mask: bits [0 .. y_bot&7]  =  0xFF >> (7 - (y_bot & 7))
        uint8_t mask = (uint8_t)(0xFFu << (y_top & 7))
                     & (uint8_t)(0xFFu >> (7 - (y_bot & 7)));
        fb[page_top][x] |= pbyte & mask;
        return;
    }

    // Top partial page: bits from y_top&7 up to bit 7
    fb[page_top][x] |= pbyte & (uint8_t)(0xFFu << (y_top & 7));

    // Middle full pages — hot path: single byte OR, no masking needed
    for (int p = page_top + 1; p < page_bot; p++)
        fb[p][x] |= pbyte;

    // Bottom partial page: bits from bit 0 down to y_bot&7
    fb[page_bot][x] |= pbyte & (uint8_t)(0xFFu >> (7 - (y_bot & 7)));
}

// ── pattern_fill_rect ─────────────────────────────────────────────────────────

void pattern_fill_rect(uint8_t fb[8][128], int x0, int y0, int w, int h, uint8_t pat)
{
    int x1 = x0 + w - 1;
    int y1 = y0 + h - 1;
    for (int x = x0; x <= x1; x++)
        pattern_vstrip(fb, x, y0, y1, pat);
}
