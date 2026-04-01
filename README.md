# ESP32-C3 3D Wireframe Engine

A dependency-free 3D wireframe engine for the ESP32-C3 + SSD1306 OLED display, written in C using ESP-IDF. Renders a tumbling wireframe cube at interactive framerates with no hardware FPU.

## Hardware

| Part | Detail |
|---|---|
| MCU | ESP32-C3 (RISC-V, 160 MHz, no FPU) |
| Display | Whadda WPI438 — SSD1306 128×64 monochrome OLED |
| SDA | GPIO9 |
| SCL | GPIO8 |

## How it works

The ESP32-C3 has no floating-point unit, so every float operation requires ~100 software-emulated cycles. This engine avoids floats entirely at runtime:

- **Q16.16 fixed-point arithmetic** — all spatial math uses 32-bit integers with a 16-bit fractional part
- **256-entry sine LUT** — trig values pre-computed at boot (the only float usage); `uint8_t` angles wrap naturally, eliminating modulo ops
- **Rodrigues axis-angle rotation** — single angle drives rotation around a fixed axis `k = normalize([3,2,1])`; Rx(45°) pre-tilt prevents any face from ever appearing face-on to the camera
- **Bresenham line rasteriser** — integer only, no multiply or divide per pixel
- **SSD1306 vertical-byte framebuffer** — `fb[page][col]`, full 1 KB frame pushed in one I2C burst per render cycle

## Requirements

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/) v5.x
- ESP32-C3 dev board
- SSD1306 128×64 I2C OLED (Whadda WPI438 or compatible)

## Build & flash

```bash
./esp.sh all              # clean + build + flash + monitor
./esp.sh build
./esp.sh flash
./esp.sh monitor
./esp.sh flash-monitor
```

The port is autodetected. Pass it explicitly if needed:

```bash
./esp.sh flash /dev/ttyUSB0
```

First build only — set the target once:

```bash
source ~/.espressif/v5.5.3/esp-idf/export.sh
idf.py set-target esp32c3
```

## Project structure

```
main/
  fixed_math.h/c   Q16.16 fixed-point types, macros, 256-entry sin LUT
  ssd1306.h/c      SSD1306 I2C driver (init + full-frame flush)
  engine3d.h/c     3D pipeline — rotation, projection, rasterisation
  main.c           Entry point — rotating cube demo
esp.sh             Build / flash / monitor helper script
```
