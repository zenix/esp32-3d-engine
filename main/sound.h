#pragma once

// ── Sound Effects ─────────────────────────────────────────────────────────────
// Driven by the ESP32-C3 LEDC peripheral (hardware PWM) on GPIO5.
// Connect a passive piezo buzzer between GPIO5 and GND (through a resistor
// or transistor if the buzzer draws more than ~10 mA).
//
// Playback is non-blocking: a one-shot esp_timer advances the note sequence
// in the background. Calling sound_play() while a sound is already playing
// immediately cuts over to the new sound.
//
// To disable sound entirely, simply don't call sound_init() — all other
// sound_* functions are safe to call without it (they become no-ops).

typedef enum {
    SFX_SHOOT       = 0,  // short laser-zap
    SFX_EXPLODE     = 1,  // longer explosion rattle
    SFX_PICKUP      = 2,  // ascending two-tone chime
    SFX_GAMEOVER    = 3,  // descending three-note phrase
    SFX_DOOR_OPEN   = 4,  // mechanical sliding rumble
    SFX_ENEMY_HURT  = 5,  // short impact hit
    SFX_PLAYER_HURT = 6,  // descending pain tone
    SFX_COUNT,
} sfx_id_t;

// Configure LEDC timer + channel on GPIO5. Call once at boot.
void sound_init(void);

// Start playing a sound effect (non-blocking, cuts over any in-progress sound).
void sound_play(sfx_id_t sfx);

// Silence immediately.
void sound_stop(void);
