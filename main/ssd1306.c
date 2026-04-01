#include "ssd1306.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define OLED_ADDR       0x3C
#define TIMEOUT_MS      100

static i2c_master_dev_handle_t g_dev;

// Send a stream of command bytes in one I2C transaction.
// SSD1306 I2C protocol: control byte 0x00 → all following bytes are commands.
static void send_cmds(const uint8_t *cmds, size_t len)
{
    // Build: [0x00, cmd0, cmd1, ...]
    uint8_t buf[32];
    buf[0] = 0x00;
    memcpy(buf + 1, cmds, len);
    i2c_master_transmit(g_dev, buf, len + 1, TIMEOUT_MS);
}

void ssd1306_init(int sda_io, int scl_io, int freq_hz)
{
    // --- I2C bus ---
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port            = I2C_NUM_0,
        .sda_io_num          = sda_io,
        .scl_io_num          = scl_io,
        .clk_source          = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt   = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    i2c_new_master_bus(&bus_cfg, &bus);

    // --- Device ---
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = OLED_ADDR,
        .scl_speed_hz    = (uint32_t)freq_hz,
    };
    i2c_master_bus_add_device(bus, &dev_cfg, &g_dev);

    // Let the display power rail stabilise.
    vTaskDelay(pdMS_TO_TICKS(10));

    // SSD1306 initialisation sequence (see datasheet §8.5 Application Example).
    static const uint8_t init[] = {
        0xAE,       // Display OFF
        0xD5, 0x80, // Clock divide ratio / oscillator frequency
        0xA8, 0x3F, // Multiplex ratio = 64 rows
        0xD3, 0x00, // Display offset = 0
        0x40,       // Display start line = 0
        0x8D, 0x14, // Charge pump: enable
        0x20, 0x00, // Memory addressing mode: horizontal
        0xA1,       // Segment re-map: col 127 → SEG0 (flip X)
        0xC8,       // COM output scan direction: remapped (flip Y)
        0xDA, 0x12, // COM pins hardware configuration
        0x81, 0xCF, // Contrast = 0xCF
        0xD9, 0xF1, // Pre-charge period
        0xDB, 0x40, // VCOMH deselect level
        0xA4,       // Entire display ON (output follows RAM)
        0xA6,       // Normal display (not inverted)
        0xAF,       // Display ON
    };
    send_cmds(init, sizeof(init));
}

// Static buffer: 1 control byte + 1024 pixel bytes.
// Avoids heap allocation on every frame.
static uint8_t g_tx_buf[1 + 8 * 128];

void ssd1306_flush(uint8_t fb[8][128])
{
    // Reset column/page pointers to cover the full display.
    static const uint8_t addr[] = {
        0x21, 0x00, 0x7F,   // Column address: 0–127
        0x22, 0x00, 0x07,   // Page address:   0–7
    };
    send_cmds(addr, sizeof(addr));

    // SSD1306 I2C data transfer: control byte 0x40 → all bytes are pixel data.
    // In horizontal addressing mode the controller auto-advances col then page.
    g_tx_buf[0] = 0x40;
    memcpy(g_tx_buf + 1, fb, 8 * 128);
    i2c_master_transmit(g_dev, g_tx_buf, sizeof(g_tx_buf), TIMEOUT_MS);
}
