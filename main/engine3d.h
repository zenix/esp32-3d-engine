#pragma once
#include <stdint.h>

// ── Mesh ──────────────────────────────────────────────────────────────────────
// Store mesh data in flash with const.  Vertices are signed model-space coords
// in int8_t; scale them up or down by adjusting their values.  Edges are pairs
// of vertex indices.
//
// Backface culling (optional — set faces/edge_face/n_faces to NULL/0 to draw
// all edges unconditionally, which preserves behaviour for existing meshes):
//   faces     — triangle faces as vertex-index triples
//   edge_face — one byte per edge: index into faces[]; 0xFF = always draw
//   n_faces   — number of entries in faces[]
typedef struct {
    const int8_t  (*verts)[3];   // model-space vertices (x, y, z)
    const uint8_t (*edges)[2];   // index pairs defining wireframe edges
    uint8_t n_verts;
    uint8_t n_edges;
    // Backface culling (optional)
    const uint8_t (*faces)[3];   // triangle faces as vertex indices
    const uint8_t (*edge_face)[2]; // edge_face[i][0..1]: up to 2 face indices per edge; 0xFF = unused slot
    uint8_t n_faces;
} mesh_t;

// ── Rotation axis presets ─────────────────────────────────────────────────────
// Used in transform_t.axis.  AXIS_DEFAULT (0) keeps the original
// k=normalize([3,2,1]) behaviour so all existing code is unaffected.
#define AXIS_DEFAULT  0   // k = normalize([3,2,1]) — original diagonal axis
#define AXIS_X        1   // k = [1,0,0]
#define AXIS_Y        2   // k = [0,1,0]
#define AXIS_Z        3   // k = [0,0,1]

// ── Transform ─────────────────────────────────────────────────────────────────
// Describes where and how to render one mesh instance.
//
//   x, y  — lateral world-space position; (0,0) centres on screen.
//   z     — depth; larger = further = smaller on screen.  Keep z > 0.
//             z ≈ 180 gives a comfortable default size for a ±25-unit mesh.
//   angle — rotation (0-255 = 0-360°).
//   axis  — rotation axis: AXIS_DEFAULT(0), AXIS_X(1), AXIS_Y(2), AXIS_Z(3).
//             0 = AXIS_DEFAULT so zero-initialised structs are unchanged.
//   scale — uniform scale: 0 or 128 = 1.0×, 64 = 0.5×, 192 = 1.5×.
//             Formula: scale_fp = (scale == 0) ? FP_ONE : (fp_t)scale << 9.
//             0 = 1.0× so zero-initialised structs are unchanged.
typedef struct {
    int16_t x, y, z;
    uint8_t angle;
    uint8_t axis;   // AXIS_DEFAULT(0) / AXIS_X(1) / AXIS_Y(2) / AXIS_Z(3)
    uint8_t scale;  // 0 or 128 = 1.0×; 64 = 0.5×; 192 = 1.5×
} transform_t;

// ── Camera ────────────────────────────────────────────────────────────────────
// Optional view transform applied to every subsequent engine3d_draw_mesh call.
// Only yaw (Y-axis) rotation is supported to keep the horizon stable.
//
//   x, y, z — camera world position (same coordinate system as transform_t).
//   yaw     — horizontal rotation (0-255 = 0-360°). Objects to the right of the
//              camera (positive X) rotate into view as yaw increases.
//
// Call engine3d_set_camera(NULL) to reset to identity (no camera).
typedef struct {
    int16_t x, y, z;
    uint8_t yaw;
} camera_t;

// Set (or clear) the active camera. Affects all engine3d_draw_mesh calls until
// changed. NULL = identity view (same behaviour as before cameras were added).
void engine3d_set_camera(const camera_t *cam);

// ── API ───────────────────────────────────────────────────────────────────────
// Prerequisite: fp_lut_init() must be called before the first draw call.

// Draw any mesh with the given transform into the 128×64 SSD1306 framebuffer.
// Supports up to ENGINE3D_MAX_VERTS vertices per mesh (defined in engine3d.c).
void engine3d_draw_mesh(uint8_t fb[8][128], const mesh_t *mesh, const transform_t *t);

// Write a single pixel into the framebuffer (bounds-checked).
// Exposed for use by font, particle, and other modules that need direct pixel access.
static inline void engine3d_draw_pixel(uint8_t fb[8][128], int x, int y)
{
    if ((unsigned)x < 128u && (unsigned)y < 64u)
        fb[y >> 3][x] |= (uint8_t)(1u << (y & 7));
}


