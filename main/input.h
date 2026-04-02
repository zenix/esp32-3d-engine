#pragma once
#include <stdbool.h>

// ── Button IDs ────────────────────────────────────────────────────────────────
// Wire each button between its GPIO and GND. Internal pull-ups are enabled.
// Default GPIO assignments (change BTN_GPIO_* below if needed):
//   UP=GPIO0  DOWN=GPIO1  LEFT=GPIO2  RIGHT=GPIO3  ACTION=GPIO4
typedef enum {
    BTN_UP     = 0,
    BTN_DOWN   = 1,
    BTN_LEFT   = 2,
    BTN_RIGHT  = 3,
    BTN_ACTION = 4,
    BTN_COUNT  = 5,
} button_id_t;

// ── API ───────────────────────────────────────────────────────────────────────

// Configure GPIO pull-ups. Call once at boot before the game loop.
void input_init(void);

// Sample all buttons and update debounce state. Call exactly once per frame.
void input_poll(void);

// True while the button is held down (debounced).
bool input_held(button_id_t b);

// True only on the frame the button transitions from up → down.
bool input_just_pressed(button_id_t b);

// True only on the frame the button transitions from down → up.
bool input_just_released(button_id_t b);
