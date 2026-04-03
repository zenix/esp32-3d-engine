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
- **Rodrigues axis-angle rotation** — per-mesh axis preset (X/Y/Z or diagonal); Rx(45°) pre-tilt prevents any face from ever appearing face-on to the camera
- **Near-plane edge clipping** — edges crossing z<10 are parametrically clipped, not clamped
- **Depth-sorted draw order** — entities sorted back-to-front each frame so closer meshes correctly overdraw farther ones
- **Bresenham line rasteriser** — integer only, no multiply or divide per pixel
- **SSD1306 vertical-byte framebuffer** — `fb[page][col]`, full 1 KB frame pushed in one I2C burst per render cycle
- **Fixed-timestep loop** — `vTaskDelayUntil()` targets 33 ms frames; entity velocities are per-frame for deterministic physics
- **DDA raycaster** — 128 rays/frame through a 2D tile grid; perpendicular-distance wall height (fisheye-free); per-column depth buffer for sprite occlusion
- **Dithered fill patterns** — 8 tiling patterns replace textures on the 1-bit display; N/S vs E/W face shading simulates directional lighting without any runtime light calculation
- **Billboard sprites** — world objects projected and scaled by distance, clipped per-column against the raycaster depth buffer

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
  meshes.h/c       Built-in game meshes — CUBE, SHIP, ASTEROID, BULLET, DIAMOND
  scene.h          Scene/state-machine types (header-only)
  game.h/c         Generic entity system and game context (no game-specific code)
  collision.h/c    Sphere-sphere collision detection
  particle.h/c     Single-pixel particle burst effects
  pattern.h/c      Dithered fill patterns — 8 styles used as "textures" on 1-bit display
  raycast.h/c      DDA raycaster — 128-ray 2.5D wall renderer with per-column depth buffer
  billboard.h/c    Billboard sprites — distance-scaled, wall-depth-clipped 2D shapes
  demo.h/c         6-page interactive feature demo
  asteroid.h/c     Asteroid Blaster game
  doom.h/c         Corridor Crawler FPS (boots by default)
  main.c           app_main — fixed-timestep game loop; wires starting scene
sdkconfig.defaults Project-level sdkconfig overrides
esp.sh             Build/flash/monitor helper
```

## What boots by default

The firmware boots into **Corridor Crawler** — a Doom-style first-person shooter.

| Control | Action |
|---|---|
| LEFT / RIGHT | Turn |
| UP / DOWN | Move forward / back |
| ACTION | Shoot |

Navigate a 20×20 tile maze, shoot enemies, and collect health and ammo pickups. Walls use dithered fill patterns to visually distinguish materials and simulate directional lighting (N/S faces appear brighter than E/W faces).

To switch to other games, change the starting scene in `main.c`:
```c
game_switch_scene(&g_game, &SCENE_ASTEROID_TITLE);  // Asteroid Blaster
game_switch_scene(&g_game, &SCENE_DEMO);            // feature demo
```

## Asteroid Blaster

A 3D space shooter. Use the D-pad to move your ship, ACTION to fire bullets, destroy asteroids for points, and collect diamonds for bonuses. Difficulty scales with score.

## Feature Demo

The demo showcases 6 pages of engine capabilities. Press **ACTION** to advance pages:

| Page | Feature demonstrated |
|---|---|
| 1/6 | All 5 built-in meshes cycling one at a time |
| 2/6 | Per-mesh scale (0.5×/1×/1.5×) and rotation axis (X/Y/Z) |
| 3/6 | Backface culling on/off comparison |
| 4/6 | Near-plane clipping — mesh drifts through z=10 cleanly |
| 5/6 | Camera yaw (LEFT/RIGHT) + depth (UP/DOWN) + depth-sorted overdraw |
| 6/6 | Collision detection + particle explosions + sound |

## Mesh API

Define any wireframe object as a `const` vertex + edge array (stored in flash):

```c
static const int8_t my_verts[][3] = { {0,20,0}, {-15,-10,0}, {15,-10,0} };
static const uint8_t my_edges[][2] = { {0,1}, {0,2}, {1,2} };
const mesh_t MY_MESH = {
    .verts = my_verts, .edges = my_edges, .n_verts = 3, .n_edges = 3,
    .faces = NULL, .edge_face = NULL, .n_faces = 0,  // NULL = no backface culling
};
```

`transform_t` fields:

| Field | Type | Description |
|---|---|---|
| `x`, `y` | `int16_t` | World position offset (0,0 = centre) |
| `z` | `int16_t` | Depth — larger = further = smaller on screen |
| `angle` | `uint8_t` | Rotation, 0–255 maps to 0–360° |
| `axis` | `uint8_t` | `AXIS_DEFAULT`/`AXIS_X`/`AXIS_Y`/`AXIS_Z` |
| `scale` | `uint8_t` | 0 or 128 = 1.0×, 64 = 0.5×, 192 = 1.5× |

All fields zero-initialise to sensible defaults (`AXIS_DEFAULT`, scale 1.0×).

Built-in meshes: `MESH_CUBE`, `MESH_CUBE_CULLED`, `MESH_SHIP`, `MESH_ASTEROID`, `MESH_BULLET`, `MESH_DIAMOND` (all in `meshes.h`). `MESH_CUBE_CULLED` has backface culling enabled; the rest are full wireframe.

## Game API

### Input

```c
input_init();
input_poll();                              // call once per frame
if (input_held(BTN_LEFT))       x -= 2;
if (input_just_pressed(BTN_ACTION)) fire();
```

### Entities

```c
static game_t g;   // must be static
game_init(&g);
game_switch_scene(&g, &MY_START_SCENE);  // wire starting scene in main.c

entity_t *e = entity_spawn(&g, &MESH_SHIP, ETYPE_PLAYER);
e->fx = INT_FP(0);  e->fz = INT_FP(160);
e->transform.axis  = AXIS_Y;
e->transform.scale = 128;
e->collision_radius = INT_FP(15);

entity_update_positions(&g);  // each frame
entity_draw_all(&g, fb);      // depth-sorted, camera-aware

entity_kill(e);
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
g.camera.yaw = player_yaw;   // 0–255 = 0–360°, Y-axis only
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
sound_play(SFX_SHOOT);        // SFX_SHOOT / EXPLODE / PICKUP / GAMEOVER
sound_play(SFX_DOOR_OPEN);    // SFX_DOOR_OPEN / ENEMY_HURT / PLAYER_HURT
```

### Particles

```c
particle_spawn_burst(x, y, z, 8);
// each frame:
particle_update();
particle_draw(fb, cam_x, cam_y, cam_z);
```

### Raycaster (2.5D FPS walls)

```c
// Define a tile map (keep cells mutable so doors can be opened)
static uint8_t my_cells[W * H];  // 0=empty, MAP_WALL_1..4=solid
raycast_map_t map = { my_cells, W, H, tile_shift };  // tile_shift=6 → 64-unit tiles

// Each frame: cast 128 rays and draw walls
raycast_render(fb, &map, player_x, player_z, yaw);

// Depth buffer for sprite occlusion (valid until next raycast_render call)
const ray_hit_t *hits = raycast_get_hits();  // hits[col].dist in Q16.16 world units
```

At `yaw=0` the player faces `+Z`. **`yaw++` turns left** (opposite of `engine3d` camera). Forward vector: `dx = -fp_sin(yaw)`, `dz = fp_cos(yaw)`.

Wall types use paired fill patterns — N/S face (brighter) and E/W face (darker) — simulating directional lighting with zero runtime cost.

### Billboard sprites

```c
// Draw a world-space sprite, depth-clipped against the raycast wall buffer.
// Call raycast_render() first each frame.
billboard_draw(fb, world_x, world_z, cam_x, cam_z, cam_yaw, SPR_ENEMY_IDLE);
// SPR_ENEMY_IDLE / SPR_ENEMY_ATCK / SPR_HEALTH / SPR_AMMO / SPR_KEY
```

Sort sprites back-to-front by view-space Z before drawing for correct overdraw ordering.

### Fill patterns

```c
// Vertical strip — hot path used by the raycaster (one call per screen column)
pattern_vstrip(fb, x, y_top, y_bot, PAT_SOLID);    // 8 pattern IDs: PAT_SOLID..PAT_DIAG_R
// Filled rectangle
pattern_fill_rect(fb, x, y, w, h, PAT_CHECK);
```
