#include "sound.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include <stddef.h>  // NULL

#define SOUND_GPIO      5
#define LEDC_SPEED      LEDC_LOW_SPEED_MODE
#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_CHANNEL    LEDC_CHANNEL_0
#define LEDC_RESOLUTION LEDC_TIMER_8_BIT   // 8-bit duty (0-255)
#define LEDC_DUTY_50PCT 128                 // 50% duty = square wave

// ── Note sequence type ────────────────────────────────────────────────────────
typedef struct {
    uint16_t freq_hz;     // frequency in Hz; 0 = end of sequence
    uint16_t duration_ms; // note duration in milliseconds
} note_t;

// ── Sound effect data (const, lives in flash) ─────────────────────────────────
static const note_t sfx_shoot[] = {
    {880, 40}, {440, 40}, {0, 0},
};
static const note_t sfx_explode[] = {
    {200, 60}, {150, 60}, {250, 60}, {100, 80}, {180, 60}, {0, 0},
};
static const note_t sfx_pickup[] = {
    {523, 60}, {659, 60}, {784, 80}, {0, 0},
};
static const note_t sfx_gameover[] = {
    {440, 150}, {330, 150}, {220, 300}, {0, 0},
};

static const note_t * const sfx_table[SFX_COUNT] = {
    sfx_shoot,
    sfx_explode,
    sfx_pickup,
    sfx_gameover,
};

// ── Playback state ────────────────────────────────────────────────────────────
static esp_timer_handle_t s_timer = NULL;
static const note_t      *s_seq   = NULL;  // current sequence
static int                s_idx   = 0;     // current note index

// ── Internal helpers ──────────────────────────────────────────────────────────
static void play_note(uint16_t freq_hz)
{
    if (freq_hz == 0) {
        ledc_set_duty(LEDC_SPEED, LEDC_CHANNEL, 0);
        ledc_update_duty(LEDC_SPEED, LEDC_CHANNEL);
        return;
    }
    ledc_set_freq(LEDC_SPEED, LEDC_TIMER, freq_hz);
    ledc_set_duty(LEDC_SPEED, LEDC_CHANNEL, LEDC_DUTY_50PCT);
    ledc_update_duty(LEDC_SPEED, LEDC_CHANNEL);
}

static void timer_cb(void *arg)
{
    if (!s_seq) return;

    s_idx++;
    if (s_seq[s_idx].freq_hz == 0) {
        // End of sequence — silence.
        ledc_set_duty(LEDC_SPEED, LEDC_CHANNEL, 0);
        ledc_update_duty(LEDC_SPEED, LEDC_CHANNEL);
        s_seq = NULL;
        return;
    }

    play_note(s_seq[s_idx].freq_hz);
    esp_timer_start_once(s_timer, (uint64_t)s_seq[s_idx].duration_ms * 1000ULL);
}

// ── Public API ────────────────────────────────────────────────────────────────
void sound_init(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = LEDC_SPEED,
        .timer_num       = LEDC_TIMER,
        .duty_resolution = LEDC_RESOLUTION,
        .freq_hz         = 440,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_cfg);

    ledc_channel_config_t chan_cfg = {
        .speed_mode = LEDC_SPEED,
        .channel    = LEDC_CHANNEL,
        .timer_sel  = LEDC_TIMER,
        .gpio_num   = SOUND_GPIO,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&chan_cfg);

    esp_timer_create_args_t timer_args = {
        .callback        = timer_cb,
        .arg             = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name            = "snd",
    };
    esp_timer_create(&timer_args, &s_timer);
}

void sound_play(sfx_id_t sfx)
{
    if (!s_timer) return; // sound_init not called
    if (sfx >= SFX_COUNT) return;

    // Stop any in-progress playback.
    esp_timer_stop(s_timer); // safe to call even if not running
    s_seq = sfx_table[sfx];
    s_idx = 0;

    if (s_seq[0].freq_hz == 0) return;

    play_note(s_seq[0].freq_hz);
    esp_timer_start_once(s_timer, (uint64_t)s_seq[0].duration_ms * 1000ULL);
}

void sound_stop(void)
{
    if (!s_timer) return;
    esp_timer_stop(s_timer);
    s_seq = NULL;
    ledc_set_duty(LEDC_SPEED, LEDC_CHANNEL, 0);
    ledc_update_duty(LEDC_SPEED, LEDC_CHANNEL);
}
