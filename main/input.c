#include "input.h"
#include "driver/gpio.h"

// GPIO number for each button (index = button_id_t value).
static const int BTN_GPIO[BTN_COUNT] = {0, 1, 2, 3, 4};

// 8-bit shift register per button: LSB = most recent sample.
// All 1s = stable released (pull-up, active-low).
// All 0s = stable pressed.
static uint8_t shift[BTN_COUNT];

// Stable debounced state bitmask: bit N set = button N is held.
static uint8_t stable;

// Previous stable state for edge detection.
static uint8_t prev_stable;

void input_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 0,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    for (int i = 0; i < BTN_COUNT; i++) {
        cfg.pin_bit_mask |= (1ULL << BTN_GPIO[i]);
        shift[i] = 0xFF; // start assumed released
    }
    gpio_config(&cfg);
    stable      = 0;
    prev_stable = 0;
}

void input_poll(void)
{
    prev_stable = stable;

    for (int i = 0; i < BTN_COUNT; i++) {
        // Shift in new sample (active-low: 0 = pressed).
        shift[i] = (uint8_t)((shift[i] << 1) | gpio_get_level(BTN_GPIO[i]));

        // Update stable state when last 3 samples agree (~99 ms at 30 fps).
        if ((shift[i] & 0x07) == 0x00) {
            stable |=  (uint8_t)(1u << i);  // consistently pressed
        } else if ((shift[i] & 0x07) == 0x07) {
            stable &= ~(uint8_t)(1u << i);  // consistently released
        }
        // Intermediate values leave stable state unchanged (transitioning).
    }
}

bool input_held(button_id_t b)
{
    return (stable >> b) & 1u;
}

bool input_just_pressed(button_id_t b)
{
    return ((stable & ~prev_stable) >> b) & 1u;
}

bool input_just_released(button_id_t b)
{
    return ((~stable & prev_stable) >> b) & 1u;
}
