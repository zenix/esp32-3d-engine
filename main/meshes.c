#include "meshes.h"

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
const mesh_t MESH_SHIP = {_ship_verts, _ship_edges, 6, 8};

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
const mesh_t MESH_ASTEROID = {_ast_verts, _ast_edges, 12, 18};

// ── MESH_BULLET — single line segment along +Z ────────────────────────────────
// 2 vertices, 1 edge. Use small z value (e.g. z=150) to make it visible.
static const int8_t _blt_verts[2][3] = {
    {0, 0,  8},  // 0 front
    {0, 0, -8},  // 1 rear
};
static const uint8_t _blt_edges[1][2] = {
    {0, 1},
};
const mesh_t MESH_BULLET = {_blt_verts, _blt_edges, 2, 1};

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
const mesh_t MESH_DIAMOND = {_dia_verts, _dia_edges, 6, 12};
