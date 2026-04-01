#pragma once
#include <stdint.h>

// Initialise I2C bus and bring the SSD1306 up (display on, horizontal
// addressing mode, full 128×64 area).
void ssd1306_init(int sda_io, int scl_io, int freq_hz);

// Push the entire 1 KB framebuffer to the display in one I2C burst.
// fb[page][col]: page 0-7, col 0-127.
// Each byte is a vertical strip of 8 pixels; bit 0 = top row of the page.
void ssd1306_flush(uint8_t fb[8][128]);
