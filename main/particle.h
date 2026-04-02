#pragma once
#include <stdint.h>
#include "fixed_math.h"

// ── Particle System ───────────────────────────────────────────────────────────
// Single-pixel particles for explosion bursts and other effects.
// Each particle has a world-space position, velocity, and lifetime.
// Rendered with the same perspective projection as meshes, so they integrate
// naturally with the 3D scene.
//
// Call order each frame:
//   particle_update();
//   particle_draw(fb, cam_x, cam_y, cam_z);

#define MAX_PARTICLES 32

// Initialise internal state. Call once at boot (or particle_init is safe to
// skip — the zero-initialised state is already valid after a memset on BSS).
void particle_init(void);

// Spawn `count` particles at world position (x, y, z) with random velocities.
// count is clamped to available free slots.
void particle_spawn_burst(fp_t x, fp_t y, fp_t z, int count);

// Advance all active particles one frame (apply velocity, decrement lifetime).
void particle_update(void);

// Project and draw active particles into the framebuffer.
// Pass the current camera world position so particles share the same view.
// Pass (0, 0, 0) if no camera is active.
void particle_draw(uint8_t fb[8][128], int16_t cam_x, int16_t cam_y, int16_t cam_z);
