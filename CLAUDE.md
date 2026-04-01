# 3D Wireframe Engine — ESP32-C3

## Hardware

| Part | Detail |
|---|---|
| MCU | ESP32-C3 (RISC-V RV32IMC, 160 MHz, no FPU) |
| Display | Whadda WPI438 — SSD1306 128×64 monochrome OLED, I2C |
| SDA | GPIO9 |
| SCL | GPIO8 |
| I2C address | 0x3C |

GPIO8/9 are strapping pins but have been tested and confirmed stable on this board.

## ESP-IDF

Version **5.5.3**, installed at `~/.espressif/v5.5.3/esp-idf`.  
Uses the **new** I2C master API (`driver/i2c_master.h`). Do not use the legacy `driver/i2c.h`.

## Build

```bash
./esp.sh all              # clean + build + flash + monitor
./esp.sh build
./esp.sh flash
./esp.sh monitor
./esp.sh flash-monitor
```

Port is autodetected. Pass an explicit port as a second argument if needed.

## Project structure

```
main/
  fixed_math.h/c   Q16.16 fixed-point types, macros, 256-entry sin LUT
  ssd1306.h/c      SSD1306 I2C driver (init + full-frame flush)
  engine3d.h/c     3D pipeline — mesh API, axis-angle rotation, perspective projection, Bresenham line
  main.c           app_main — rotating cube demo
esp.sh             Build/flash/monitor helper
```

## Mesh API

```c
// Define a mesh (keep data const — it lives in flash)
static const int8_t my_verts[][3] = { ... };
static const uint8_t my_edges[][2] = { ... };
const mesh_t MY_MESH = { my_verts, my_edges, n_verts, n_edges };

// Draw it each frame
transform_t t = { .x = 0, .y = 0, .z = 180, .angle = angle };
engine3d_draw_mesh(fb, &MY_MESH, &t);
```

`mesh_t` holds any geometry up to `ENGINE3D_MAX_VERTS` (64) vertices.  
`transform_t` fields: `x`/`y` shift on screen, `z` controls depth/size (larger = smaller), `angle` drives rotation (0–255 = 0–360°).  
Multiple meshes can be drawn per frame by calling `engine3d_draw_mesh` multiple times before `ssd1306_flush`.  
`MESH_CUBE` is provided as a built-in (±25-unit cube, 8 verts / 12 edges).

## Key design rules

- **No floating-point at runtime.** The ESP32-C3 has no FPU; float ops cost ~100 cycles each. All math uses Q16.16 fixed-point (`fp_t`). Float is only used once at boot to populate `sin_lut[]`.
- **256-step trig LUT.** Angles are `uint8_t` (0–255 = 0–360°). Overflow wraps for free. `fp_cos(a) = sin_lut[a + 64]`.
- **SSD1306 vertical-byte layout.** `fb[page][col]`, bit 0 = top row of the page. Pixel write: `fb[y>>3][x] |= 1 << (y & 7)`.
- **One I2C burst per frame.** `ssd1306_flush` sends all 1025 bytes (control byte + 1024 pixel bytes) in a single `i2c_master_transmit` call. Avoid splitting it.
- **No division in the hot path.** Use `FP_MUL` with a reciprocal, or `FP_DIV` only once per vertex (perspective divide). Never divide per pixel.
- **Bresenham line rasteriser** — integer only, no multiply, no divide per pixel.

## Rotation design

Do **not** revert to three independent Euler angles (ax, ay, az incremented separately). That approach causes periodic face-on views where the cube looks flat, because independent angle increments create degenerate combined rotations.

The current approach uses two stages:

1. **Constant Rx(45°) pre-tilt** — applied per-vertex inside `engine3d_draw_mesh` using the compile-time constant `BASE_SC = 46341` (sin/cos 45° in Q16.16). This shifts all face normals off the Z (view) axis so no face can ever appear directly face-on to the camera.
2. **Rodrigues axis-angle rotation** around fixed axis `k = normalize([3,2,1])` — a single `uint8_t angle` from `transform_t` drives the rotation. The matrix is built once per `engine3d_draw_mesh` call and applied to all vertices.

`fp_lut_init()` must be called before the first `engine3d_draw_mesh()` call. There is no separate engine init step.
