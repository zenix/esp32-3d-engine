#pragma once
#include "scene.h"

// ── Doom-like FPS ─────────────────────────────────────────────────────────────
// Three scenes: title → play → game-over → (retry → play).
// Wire SCENE_DOOM_TITLE as the boot scene in main.c.

extern const scene_t SCENE_DOOM_TITLE;
extern const scene_t SCENE_DOOM_PLAY;
extern const scene_t SCENE_DOOM_OVER;
