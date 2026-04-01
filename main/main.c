// ESP32-C3 · 3D Wireframe Engine · Proof of Concept
//
// Hardware:
//   SSD1306 OLED 128×64 (Whadda WPI438) via I2C
//   SDA → GPIO9,  SCL → GPIO8   (tested and confirmed working)
//
// Build:
//   export IDF_PATH=~/.espressif/v5.5.3/esp-idf
//   source $IDF_PATH/export.sh
//   idf.py set-target esp32c3
//   idf.py build flash monitor
//
// What you should see: a wireframe cube tumbling continuously on the display.

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "fixed_math.h"
#include "ssd1306.h"
#include "engine3d.h"

#define SDA_IO      9
#define SCL_IO      8
// 400 kHz (Fast Mode) is the SSD1306 spec max.
// Increase to 800000 or 1000000 to push framerate higher;
// tested reliable up to ~1 MHz on most WPI438 modules.
#define I2C_FREQ_HZ 400000

void app_main(void)
{
    fp_lut_init();
    ssd1306_init(SDA_IO, SCL_IO, I2C_FREQ_HZ);

    uint8_t angle = 0;
    uint8_t fb[8][128];

    for (;;) {
        memset(fb, 0, sizeof(fb));

        transform_t t = {
            .x     = 0,
            .y     = 0,
            .z     = 180,
            .angle = angle,
        };
        engine3d_draw_mesh(fb, &MESH_CUBE, &t);
        ssd1306_flush(fb);

        angle += 2;
        vTaskDelay(1);
    }
}
