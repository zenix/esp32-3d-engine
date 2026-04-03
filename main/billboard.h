#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "fixed_math.h"

// ── Billboard Sprite Renderer ─────────────────────────────────────────────────
// Draws 2D sprites that always face the camera (billboards), depth-clipped
// against the raycaster's per-column z-buffer.
//
// Each sprite type is a 16×16 bitmap (uint16_t cols[16]).  The sprite is
// projected to screen-space, scaled by distance, and rendered with a
// clear-then-draw pass: the full sprite footprint is blacked out first, then
// only opaque pixels are lit white.  This keeps sprites visible against any
// wall pattern (solid, checkerboard, etc.) via a natural black-border cutout.
//
// Sprite center is at world Y = 0 and appears vertically centered on the
// screen horizon (y = 32), matching the player eye height.

// ── Sprite type IDs ───────────────────────────────────────────────────────────
#define SPR_ENEMY_IDLE   0   // Skull / humanoid silhouette
#define SPR_ENEMY_ATCK   1   // Humanoid with arms raised
#define SPR_HEALTH       2   // Cross / plus symbol
#define SPR_AMMO         3   // Wide flat rectangle
#define SPR_KEY          4   // Key silhouette
#define SPR_COUNT        5

// ── API ───────────────────────────────────────────────────────────────────────

// Draw a billboard sprite at world position (wx, wz) as seen from a camera
// at (cam_x, cam_z) with the given yaw.
//
// Returns true if any part of the sprite was visible (useful for gameplay
// queries like "can the enemy see me?").
//
// Prerequisite: raycast_render() must have been called this frame so that
// raycast_get_hits() returns a valid depth buffer.
bool billboard_draw(uint8_t fb[8][128],
                    fp_t wx, fp_t wz,
                    fp_t cam_x, fp_t cam_z, uint8_t cam_yaw,
                    uint8_t sprite_id);
