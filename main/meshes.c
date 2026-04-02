#include "meshes.h"
#include <stddef.h>  // NULL

// ── MESH_CUBE — ±25-unit wireframe cube ───────────────────────────────────────
// 8 verts, 12 edges, 12 tri-faces (2 per quad face). CCW winding from outside.
//
// Vertex layout:
//   0=(-25,-25,-25)  1=(+25,-25,-25)  2=(+25,+25,-25)  3=(-25,+25,-25)  back
//   4=(-25,-25,+25)  5=(+25,-25,+25)  6=(+25,+25,+25)  7=(-25,+25,+25)  front
//
// Edges 0-3: back ring  4-7: front ring  8-11: pillars
// Faces: f0/f1=front(+Z)  f2/f3=back(-Z)  f4/f5=right(+X)
//        f6/f7=left(-X)   f8/f9=top(+Y)   f10/f11=bottom(-Y)
static const int8_t _cube_verts[8][3] = {
    {-25,-25,-25}, { 25,-25,-25}, { 25, 25,-25}, {-25, 25,-25},
    {-25,-25, 25}, { 25,-25, 25}, { 25, 25, 25}, {-25, 25, 25},
};
static const uint8_t _cube_edges[12][2] = {
    {0,1},{1,2},{2,3},{3,0},   // 0-3  back ring
    {4,5},{5,6},{6,7},{7,4},   // 4-7  front ring
    {0,4},{1,5},{2,6},{3,7},   // 8-11 pillars
};
const mesh_t MESH_CUBE = {
    .verts     = _cube_verts,
    .edges     = _cube_edges,
    .n_verts   = 8,
    .n_edges   = 12,
    .faces     = NULL,
    .edge_face = NULL,
    .n_faces   = 0,
};

// MESH_CUBE with backface culling enabled — for culling demos.
static const uint8_t _cube_faces[12][3] = {
    {4,5,6},{4,6,7},   // f0,f1  front (+Z)
    {1,0,3},{1,3,2},   // f2,f3  back  (-Z)
    {5,1,2},{5,2,6},   // f4,f5  right (+X)
    {0,4,7},{0,7,3},   // f6,f7  left  (-X)
    {3,7,6},{3,6,2},   // f8,f9  top   (+Y)
    {4,0,1},{4,1,5},   // f10,f11 bottom (-Y)
};
static const uint8_t _cube_edge_face[12] = {
    2, 3, 3, 2,    // edges 0-3: back ring
    0, 1, 1, 0,    // edges 4-7: front ring
    6, 4, 5, 7,    // edges 8-11: pillars
};
const mesh_t MESH_CUBE_CULLED = {
    .verts     = _cube_verts,
    .edges     = _cube_edges,
    .n_verts   = 8,
    .n_edges   = 12,
    .faces     = _cube_faces,
    .edge_face = _cube_edge_face,
    .n_faces   = 12,
};

// ── MESH_SHIP — arrowhead/wedge pointing +Z ───────────────────────────────────
// 6 vertices, 8 edges. Fits int8_t. Looks like a spaceship from the side.
static const int8_t _ship_verts[6][3] = {
    {  0,  0,  25},   // 0 nose
    {-15,  0, -15},   // 1 left wing tip
    { 15,  0, -15},   // 2 right wing tip
    {  0,  8,  -8},   // 3 top fin
    {  0, -8,  -8},   // 4 bottom fin
    {  0,  0, -15},   // 5 tail centre
};
static const uint8_t _ship_edges[8][2] = {
    {0, 1}, {0, 2},          // nose to wings
    {1, 5}, {2, 5},          // wings to tail
    {0, 3}, {0, 4},          // nose to fins
    {3, 5}, {4, 5},          // fins to tail
};
const mesh_t MESH_SHIP = {
    .verts = _ship_verts, .edges = _ship_edges, .n_verts = 6, .n_edges = 8,
    .faces = NULL, .edge_face = NULL, .n_faces = 0,
};

// ── MESH_ASTEROID — irregular rock ───────────────────────────────────────────
// 12 vertices, 18 edges. Approximates a lumpy sphere.
static const int8_t _ast_verts[12][3] = {
    {  0,  25,   0},  // 0 top
    {  0, -25,   0},  // 1 bottom
    { 20,   8,   5},  // 2
    {-18,  10,  -3},  // 3
    {  5,   5,  22},  // 4
    { -8,   3, -23},  // 5
    { 22,  -8,  -5},  // 6
    {-20,  -6,   7},  // 7
    {  6,  -5,  21},  // 8
    { -5,  -8, -20},  // 9
    { 15,  15, -10},  // 10
    {-15, -15,  10},  // 11
};
static const uint8_t _ast_edges[18][2] = {
    { 0,  2}, { 0,  3}, { 0,  4}, { 0, 10},
    { 1,  6}, { 1,  7}, { 1,  8}, { 1, 11},
    { 2,  4}, { 2,  6}, { 2, 10},
    { 3,  5}, { 3,  7}, { 3, 10},
    { 4,  8}, { 5,  9},
    { 6,  9}, { 7, 11},
};
const mesh_t MESH_ASTEROID = {
    .verts = _ast_verts, .edges = _ast_edges, .n_verts = 12, .n_edges = 18,
    .faces = NULL, .edge_face = NULL, .n_faces = 0,
};

// ── MESH_BULLET — single line segment along +Z ────────────────────────────────
// 2 vertices, 1 edge. Use small z value (e.g. z=150) to make it visible.
static const int8_t _blt_verts[2][3] = {
    {0, 0,  8},  // 0 front
    {0, 0, -8},  // 1 rear
};
static const uint8_t _blt_edges[1][2] = {
    {0, 1},
};
const mesh_t MESH_BULLET = {
    .verts = _blt_verts, .edges = _blt_edges, .n_verts = 2, .n_edges = 1,
    .faces = NULL, .edge_face = NULL, .n_faces = 0,
};

// ── MESH_DIAMOND — octahedron for pickups/collectibles ────────────────────────
// 6 vertices, 12 edges.
static const int8_t _dia_verts[6][3] = {
    {  0,  20,   0},  // 0 top
    {  0, -20,   0},  // 1 bottom
    { 15,   0,   0},  // 2 right
    {-15,   0,   0},  // 3 left
    {  0,   0,  15},  // 4 front
    {  0,   0, -15},  // 5 back
};
static const uint8_t _dia_edges[12][2] = {
    {0, 2}, {0, 3}, {0, 4}, {0, 5},  // top to equator
    {1, 2}, {1, 3}, {1, 4}, {1, 5},  // bottom to equator
    {2, 4}, {4, 3}, {3, 5}, {5, 2},  // equator ring
};
const mesh_t MESH_DIAMOND = {
    .verts = _dia_verts, .edges = _dia_edges, .n_verts = 6, .n_edges = 12,
    .faces = NULL, .edge_face = NULL, .n_faces = 0,
};
