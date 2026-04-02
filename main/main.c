// ESP32-C3 · 3D Wireframe Game Engine
//
// Hardware:
//   SSD1306 OLED 128×64 (Whadda WPI438) via I2C
//   SDA → GPIO9,  SCL → GPIO8
//   Buttons: UP=GPIO0  DOWN=GPIO1  LEFT=GPIO2  RIGHT=GPIO3  ACTION=GPIO4
//   Buzzer (passive piezo): GPIO5
//
// Build:
//   ./esp.sh all

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "fixed_math.h"
#include "ssd1306.h"
#include "engine3d.h"
#include "input.h"
#include "sound.h"
#include "particle.h"
#include "game.h"
#include "asteroid.h"

#define SDA_IO      9
#define SCL_IO      8
#define I2C_FREQ_HZ 400000

// Target frame rate: 30 FPS → 33 ms per frame.
// vTaskDelayUntil with configTICK_RATE_HZ=100 gives 3 ticks = 30 ms actual.
// The fixed dt below is calibrated to 33 ms for deterministic physics.
#define FRAME_MS        33
#define FRAME_DT_FP     2162  // INT_FP(33) / INT_FP(1000) pre-computed in Q16.16

// The game context is too large for the stack — declare static.
static game_t g_game;

void app_main(void)
{
    fp_lut_init();
    ssd1306_init(SDA_IO, SCL_IO, I2C_FREQ_HZ);
    input_init();
    sound_init();
    particle_init();
    game_init(&g_game);
    game_switch_scene(&g_game, &SCENE_ASTEROID_TITLE);

    TickType_t prev_wake = xTaskGetTickCount();

    for (;;) {
        // ── Input ─────────────────────────────────────────────────────────────
        input_poll();

        // ── Update ────────────────────────────────────────────────────────────
        scene_update(g_game.current_scene, &g_game, FRAME_DT_FP);
        particle_update();

        // ── Render ────────────────────────────────────────────────────────────
        memset(g_game.fb, 0, sizeof(g_game.fb));
        scene_render(g_game.current_scene, &g_game, g_game.fb);
        particle_draw(g_game.fb,
                      g_game.camera_active ? g_game.camera.x : 0,
                      g_game.camera_active ? g_game.camera.y : 0,
                      g_game.camera_active ? g_game.camera.z : 0);
        ssd1306_flush(g_game.fb);

        g_game.frame_count++;

        // ── Timing: wait for next frame slot ─────────────────────────────────
        vTaskDelayUntil(&prev_wake, pdMS_TO_TICKS(FRAME_MS));
    }
}
