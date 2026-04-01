#pragma once
#include <stdint.h>

// Must be called once after fp_lut_init() — pre-computes the 45°-tilted
// vertex positions used by every subsequent draw call.
void engine3d_init(void);

// Draw a wireframe cube rotated by `angle` (uint8_t, 0-255 = 0-360°) into the
// 128×64 SSD1306 framebuffer fb[page][col].
void engine3d_draw_cube(uint8_t fb[8][128], uint8_t angle);
