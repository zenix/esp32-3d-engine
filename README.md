# ESP32-C3 3D Wireframe Game Engine

A dependency-free 3D wireframe game engine for the ESP32-C3 + SSD1306 OLED display, written in C using ESP-IDF. Renders wireframe meshes at ~30 FPS with a full game API — entities, scenes, input, collision, font, sound, and particles — all with no hardware FPU.

## Hardware

| Part | Detail |
|---|---|
| MCU | ESP32-C3 (RISC-V, 160 MHz, no FPU) |
| Display | Whadda WPI438 — SSD1306 128×64 monochrome OLED |
| SDA | GPIO9 |
| SCL | GPIO8 |
| Buttons | GPIO0–4 (UP / DOWN / LEFT / RIGHT / ACTION) |
| Buzzer | GPIO5 — passive piezo, LEDC PWM |

## How it works

The ESP32-C3 has no floating-point unit, so every float operation requires ~100 software-emulated cycles. This engine avoids floats entirely at runtime:

- **Q16.16 fixed-point arithmetic** — all spatial math uses 32-bit integers with a 16-bit fractional part
- **256-entry sine LUT** — trig values pre-computed at boot (the only float usage); `uint8_t` angles wrap naturally, eliminating modulo ops
- **Rodrigues axis-angle rotation** — single angle drives rotation around a fixed axis `k = normalize([3,2,1])`; Rx(45°) pre-tilt prevents any face from ever appearing face-on to the camera
- **Bresenham line rasteriser** — integer only, no multiply or divide per pixel
- **SSD1306 vertical-byte framebuffer** — `fb[page][col]`, full 1 KB frame pushed in one I2C burst per render cycle
- **Fixed-timestep loop** — `vTaskDelayUntil()` targets 33 ms frames; entity velocities are per-frame for deterministic physics

## Requirements

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/) v5.x
- ESP32-C3 dev board
- SSD1306 128×64 I2C OLED (Whadda WPI438 or compatible)
- 5× tactile buttons (optional — engine runs without input hardware)
- Passive piezo buzzer (optional)

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
  engine3d.h/c     3D pipeline — mesh API, camera, rotation, projection, rasterisation
  input.h/c        Button input — debounced held/just_pressed/just_released
  font.h/c         5×7 bitmap font — draw_char, draw_string, draw_int
  sound.h/c        LEDC PWM buzzer — non-blocking sound effects
  meshes.h/c       Built-in game meshes — SHIP, ASTEROID, BULLET, DIAMOND
  scene.h          Scene/state-machine types (header-only)
  game.h/c         Entity system, game context, built-in scenes
  collision.h/c    Sphere-sphere and AABB collision detection
  particle.h/c     Single-pixel particle burst effects
  main.c           app_main — fixed-timestep game loop
sdkconfig.defaults Project-level sdkconfig overrides
esp.sh             Build/flash/monitor helper
```

## Mesh API

Define any wireframe object as a `const` vertex + edge array (stored in flash):

```c
static const int8_t ship_verts[][3] = {
    { 0, 20, 0}, {-15,-10, 0}, { 15,-10, 0}, { 0, -5, 0},
};
static const uint8_t ship_edges[][2] = {
    {0,1}, {0,2}, {1,3}, {2,3},
};
const mesh_t MESH_SHIP = {ship_verts, ship_edges, 4, 4};
```

`transform_t` fields:

| Field | Description |
|---|---|
| `x`, `y` | Screen position offset (0,0 = centre) |
| `z` | Depth — larger = further away = smaller on screen |
| `angle` | Rotation, 0–255 maps to 0–360° |

Up to 64 vertices per mesh. Multiple meshes can be drawn each frame.

Built-in meshes: `MESH_CUBE`, `MESH_SHIP`, `MESH_ASTEROID`, `MESH_BULLET`, `MESH_DIAMOND`.

## Game API

### Input

```c
input_init();
// in game loop:
input_poll();
if (input_held(BTN_LEFT))        player.x -= 2;
if (input_just_pressed(BTN_ACTION)) fire_bullet();
```

### Entities

```c
static game_t g;   // must be static
game_init(&g);

entity_t *ship = entity_spawn(&g, &MESH_SHIP, ETYPE_PLAYER);
ship->fx = INT_FP(0);
ship->fz = INT_FP(160);
ship->collision_radius = INT_FP(15);

// each frame:
entity_update_positions(&g);
entity_draw_all(&g, fb);

entity_kill(ship);
```

### Scenes

```c
const scene_t MY_SCENE = {
    .on_enter = enter_fn,
    .on_exit  = exit_fn,
    .update   = update_fn,   // void(game_t*, fp_t dt)
    .render   = render_fn,   // void(game_t*, uint8_t fb[8][128])
};
game_switch_scene(&g, &MY_SCENE);
```

Built-in: `SCENE_TITLE`, `SCENE_GAMEPLAY`, `SCENE_PAUSE`, `SCENE_GAMEOVER`.

### Collision

```c
collision_check_all(&g, on_hit);

void on_hit(entity_t *a, entity_t *b, game_t *g) {
    if (a->type == ETYPE_BULLET && b->type == ETYPE_ENEMY) {
        entity_kill(a); entity_kill(b);
        g->score += 100;
    }
}
```

### Camera

```c
g.camera.x   = player_x;
g.camera.yaw = player_yaw;
g.camera_active = true;
// entity_draw_all applies it automatically
```

### Font

```c
font_draw_string(fb, 0, 0, "SCORE:");
font_draw_int(fb, 42, 0, g.score);
```

### Sound

```c
sound_init();
sound_play(SFX_SHOOT);     // SFX_SHOOT / EXPLODE / PICKUP / GAMEOVER
```

### Particles

```c
particle_spawn_burst(x, y, z, 8);
// each frame:
particle_update();
particle_draw(fb, cam_x, cam_y, cam_z);
```
