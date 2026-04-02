# 3D Wireframe Engine — ESP32-C3

## Hardware

| Part | Detail |
|---|---|
| MCU | ESP32-C3 (RISC-V RV32IMC, 160 MHz, no FPU) |
| Display | Whadda WPI438 — SSD1306 128×64 monochrome OLED, I2C |
| SDA | GPIO9 |
| SCL | GPIO8 |
| I2C address | 0x3C |
| Buttons | UP=GPIO0, DOWN=GPIO1, LEFT=GPIO2, RIGHT=GPIO3, ACTION=GPIO4 (pull-up, active-low) |
| Buzzer | Passive piezo on GPIO5 (LEDC PWM) |

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
  engine3d.h/c     3D pipeline — mesh API, camera, rotation, projection, Bresenham line
  input.h/c        Button input — debounced held/just_pressed/just_released
  font.h/c         5×7 bitmap font — draw_char, draw_string, draw_int
  sound.h/c        LEDC PWM buzzer — non-blocking sound effects via esp_timer
  meshes.h/c       Built-in game meshes — SHIP, ASTEROID, BULLET, DIAMOND
  scene.h          Scene/state-machine types (header-only)
  game.h/c         Entity system, game context, built-in scenes
  collision.h/c    Sphere-sphere and AABB collision detection
  particle.h/c     Single-pixel particle burst effects
  main.c           app_main — fixed-timestep game loop
sdkconfig.defaults Project-level sdkconfig overrides (stack size, tick rate)
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

Built-in meshes: `MESH_CUBE` (engine3d.h), `MESH_SHIP`, `MESH_ASTEROID`, `MESH_BULLET`, `MESH_DIAMOND` (meshes.h).

## Camera API

```c
camera_t cam = { .x = player_x, .y = 0, .z = 0, .yaw = player_angle };
engine3d_set_camera(&cam);           // apply before draw calls
engine3d_set_camera(NULL);           // reset to identity view
```

Camera applies inverse-translate + yaw (Y-axis) rotation to every vertex. Pitch/roll are not supported to keep the horizon stable. Set once per frame; affects all subsequent `engine3d_draw_mesh` calls until changed.

## Input API

```c
input_init();                        // call once at boot
input_poll();                        // call once per frame
input_held(BTN_LEFT);                // true while button is down
input_just_pressed(BTN_ACTION);      // true on the press edge only
input_just_released(BTN_UP);         // true on the release edge only
```

Buttons: `BTN_UP`, `BTN_DOWN`, `BTN_LEFT`, `BTN_RIGHT`, `BTN_ACTION`.

## Font API

```c
font_draw_string(fb, x, y, "SCORE"); // null-terminated string
font_draw_int(fb, x, y, 1250);       // signed decimal, no printf needed
font_draw_char(fb, x, y, 'A');       // single character
```

5×7 font, ASCII 32–126, 6 pixels per character cell. Page-aligned (y % 8 == 0) is the fast path.

## Sound API

```c
sound_init();                        // call once at boot (configures LEDC on GPIO5)
sound_play(SFX_SHOOT);               // non-blocking, cuts over current sound
sound_play(SFX_EXPLODE);
sound_play(SFX_PICKUP);
sound_play(SFX_GAMEOVER);
sound_stop();                        // silence immediately
```

## Entity / Scene API

### Game context

```c
static game_t g;   // MUST be static — too large for the stack
game_init(&g);     // clears state, enters SCENE_TITLE
```

### Entities

```c
entity_t *e = entity_spawn(&g, &MESH_SHIP, ETYPE_PLAYER);
e->fx = INT_FP(x);   e->fy = INT_FP(y);   e->fz = INT_FP(z);
e->vx = INT_FP(2);   // velocity in Q16.16 units/frame
e->collision_radius = INT_FP(15);
entity_update_positions(&g);   // apply velocity, sync transform
entity_draw_all(&g, fb);       // render all active entities
entity_kill(e);                // deactivate slot
```

`MAX_ENTITIES` = 16. Entity types: `ETYPE_NONE`, `ETYPE_PLAYER`, `ETYPE_ENEMY`, `ETYPE_BULLET`, `ETYPE_PICKUP`.

### Scenes

```c
const scene_t MY_SCENE = {
    .on_enter = my_enter,   // called once on transition in
    .on_exit  = my_exit,    // called once on transition out
    .update   = my_update,  // called each frame with dt (Q16.16 seconds)
    .render   = my_render,  // called each frame to fill fb
};
game_switch_scene(&g, &MY_SCENE);
```

Built-in scenes: `SCENE_TITLE`, `SCENE_GAMEPLAY`, `SCENE_PAUSE`, `SCENE_GAMEOVER`.

### Collision

```c
collision_check_all(&g, on_hit_cb);    // checks all active entity pairs

void on_hit_cb(entity_t *a, entity_t *b, game_t *g) {
    if (a->type == ETYPE_BULLET && b->type == ETYPE_ENEMY) { ... }
}
```

Also available: `collision_check_spheres(a, b)` for manual pair checks.

### Particles

```c
particle_spawn_burst(x, y, z, 8);              // 8 pixels explode outward
particle_update();                             // advance physics (call each frame)
particle_draw(fb, cam.x, cam.y, cam.z);        // project and draw
```

## Key design rules

- **No floating-point at runtime.** The ESP32-C3 has no FPU; float ops cost ~100 cycles each. All math uses Q16.16 fixed-point (`fp_t`). Float is only used once at boot to populate `sin_lut[]`.
- **256-step trig LUT.** Angles are `uint8_t` (0–255 = 0–360°). Overflow wraps for free. `fp_cos(a) = sin_lut[a + 64]`.
- **SSD1306 vertical-byte layout.** `fb[page][col]`, bit 0 = top row of the page. Pixel write: `fb[y>>3][x] |= 1 << (y & 7)`. Use `engine3d_draw_pixel(fb, x, y)` from other modules.
- **One I2C burst per frame.** `ssd1306_flush` sends all 1025 bytes in a single `i2c_master_transmit` call. Avoid splitting it.
- **No division in the hot path.** Use `FP_MUL` with a reciprocal, or `FP_DIV` only once per vertex (perspective divide). Never divide per pixel.
- **Bresenham line rasteriser** — integer only, no multiply, no divide per pixel.
- **`game_t` must be static.** At ~1.7 KB it exceeds the 3584-byte main task stack when combined with call overhead. Always declare it `static`.
- **Fixed timestep.** The main loop uses `vTaskDelayUntil()` targeting 33 ms frames. Entity velocities are calibrated per-frame, not per-second, so physics is deterministic regardless of render time.

## Rotation design

Do **not** revert to three independent Euler angles (ax, ay, az incremented separately). That approach causes periodic face-on views where the cube looks flat, because independent angle increments create degenerate combined rotations.

The current approach uses two stages:

1. **Constant Rx(45°) pre-tilt** — applied per-vertex inside `engine3d_draw_mesh` using the compile-time constant `BASE_SC = 46341` (sin/cos 45° in Q16.16). This shifts all face normals off the Z (view) axis so no face can ever appear directly face-on to the camera.
2. **Rodrigues axis-angle rotation** around fixed axis `k = normalize([3,2,1])` — a single `uint8_t angle` from `transform_t` drives the rotation. The matrix is built once per `engine3d_draw_mesh` call and applied to all vertices.

`fp_lut_init()` must be called before the first `engine3d_draw_mesh()` call. There is no separate engine init step.
