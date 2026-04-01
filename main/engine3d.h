#pragma once
#include <stdint.h>

// ── Mesh ──────────────────────────────────────────────────────────────────────
// Store mesh data in flash with const.  Vertices are signed model-space coords
// in int8_t; scale them up or down by adjusting their values.  Edges are pairs
// of vertex indices.
typedef struct {
    const int8_t  (*verts)[3];  // model-space vertices (x, y, z)
    const uint8_t (*edges)[2];  // index pairs defining wireframe edges
    uint8_t n_verts;
    uint8_t n_edges;
} mesh_t;

// ── Transform ─────────────────────────────────────────────────────────────────
// Describes where and how to render one mesh instance.
//
//   x, y  — lateral world-space position; (0,0) centres on screen.
//   z     — depth; larger = further = smaller on screen.  Keep z > 0.
//             z ≈ 180 gives a comfortable default size for a ±25-unit mesh.
//   angle — rotation (0-255 = 0-360°).  All objects rotate around the same
//             fixed axis k=normalize([3,2,1]) with an Rx(45°) pre-tilt baked
//             in, so no face ever appears directly face-on to the camera.
typedef struct {
    int16_t x, y, z;
    uint8_t angle;
} transform_t;

// ── API ───────────────────────────────────────────────────────────────────────
// Prerequisite: fp_lut_init() must be called before the first draw call.

// Draw any mesh with the given transform into the 128×64 SSD1306 framebuffer.
// Supports up to ENGINE3D_MAX_VERTS vertices per mesh (defined in engine3d.c).
void engine3d_draw_mesh(uint8_t fb[8][128], const mesh_t *mesh, const transform_t *t);

// ── Built-in meshes ───────────────────────────────────────────────────────────
extern const mesh_t MESH_CUBE;      // ±25-unit wireframe cube, 8 verts / 12 edges
