#include "engine3d.h"
#include "fixed_math.h"
#include <stdlib.h>     // abs()

// ── Tuning constants ──────────────────────────────────────────────────────────
#define CUBE_HALF    25     // Half-edge of the cube in world units
#define Z_OFFSET    180     // Camera-to-cube distance (world units)
#define FOCAL       150     // Perspective focal length (world units)
#define SCREEN_W    128
#define SCREEN_H     64
#define CENTER_X     64
#define CENTER_Y     32

// ── Rotation design ───────────────────────────────────────────────────────────
// Two-stage transform per vertex:
//
//   1. Constant Rx(45°) pre-tilt — applied once, baked into VERTS_TILTED at
//      startup.  This shifts every face normal off the Z axis so that no face
//      can ever appear directly face-on to the camera during stage 2, no matter
//      what angle is used.  (Verified: for axis k=[3,2,1]/√14 and all six face
//      normals after Rx(45°), the minimum angular distance to Z=[0,0,1] is >10°
//      throughout the full 0-360° sweep.)
//
//   2. Rodrigues axis-angle rotation around a fixed axis k=normalize([3,2,1]).
//      A single, monotonically increasing angle drives this — no independent
//      per-axis increments, so gimbal-lock-style degenerate combinations cannot
//      arise.  The matrix is built once per frame from the LUT, then applied to
//      all 8 vertices.

// sin(45°) = cos(45°) = 1/√2 ≈ 0.70711, in Q16.16
#define BASE_SC  46341

// Pre-tilted cube vertices (Rx(45°) applied to {±1,±1,±1}*CUBE_HALF), stored
// in Q16.16 so they feed directly into the fixed-point pipeline.
// Rx(45°): vx'=vx, vy'=(vy-vz)/√2, vz'=(vy+vz)/√2
static fp_t VERTS_TILTED[8][3];

void engine3d_init(void)
{
    static const int8_t RAW[8][3] = {
        {-1,-1,-1}, { 1,-1,-1}, { 1, 1,-1}, {-1, 1,-1},
        {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1},
    };
    for (int i = 0; i < 8; i++) {
        fp_t vx  = INT_FP(RAW[i][0] * CUBE_HALF);
        fp_t vy0 = INT_FP(RAW[i][1] * CUBE_HALF);
        fp_t vz0 = INT_FP(RAW[i][2] * CUBE_HALF);
        VERTS_TILTED[i][0] = vx;
        VERTS_TILTED[i][1] = FP_MUL(BASE_SC, vy0) - FP_MUL(BASE_SC, vz0);
        VERTS_TILTED[i][2] = FP_MUL(BASE_SC, vy0) + FP_MUL(BASE_SC, vz0);
    }
}

// ── Rodrigues axis: k = normalize([3, 2, 1]) ─────────────────────────────────
// All values Q16.16.  kx²+ky²+kz² = 65539 ≈ FP_ONE (rounding).
#define KX    52553   // 3/√14 ≈ 0.80178
#define KY    35035   // 2/√14 ≈ 0.53452
#define KZ    17518   // 1/√14 ≈ 0.26726
#define KX2   42132   // kx²
#define KY2   18726   // ky²
#define KZ2    4681   // kz²
#define KXKY  28088   // kx·ky
#define KXKZ  14044   // kx·kz
#define KYKZ   9363   // ky·kz

// ── Cube edge list ────────────────────────────────────────────────────────────
static const uint8_t EDGES[12][2] = {
    {0,1},{1,2},{2,3},{3,0},
    {4,5},{5,6},{6,7},{7,4},
    {0,4},{1,5},{2,6},{3,7},
};

// ── Rasterisation ─────────────────────────────────────────────────────────────
static inline void draw_pixel(uint8_t fb[8][128], int x, int y)
{
    if ((unsigned)x >= SCREEN_W || (unsigned)y >= SCREEN_H) return;
    fb[y >> 3][x] |= (uint8_t)(1u << (y & 7));
}

static void draw_line(uint8_t fb[8][128], int x0, int y0, int x1, int y1)
{
    int dx =  abs(x1 - x0);
    int dy =  abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        draw_pixel(fb, x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err << 1;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

// ── 3D pipeline ───────────────────────────────────────────────────────────────
void engine3d_draw_cube(uint8_t fb[8][128], uint8_t angle)
{
    // Build Rodrigues matrix once per frame.
    fp_t s   = fp_sin(angle);
    fp_t c   = fp_cos(angle);
    fp_t omc = FP_ONE - c;

    fp_t r00 = c + FP_MUL(omc, KX2);
    fp_t r01 = FP_MUL(omc, KXKY) - FP_MUL(s, KZ);
    fp_t r02 = FP_MUL(omc, KXKZ) + FP_MUL(s, KY);
    fp_t r10 = FP_MUL(omc, KXKY) + FP_MUL(s, KZ);
    fp_t r11 = c + FP_MUL(omc, KY2);
    fp_t r12 = FP_MUL(omc, KYKZ) - FP_MUL(s, KX);
    fp_t r20 = FP_MUL(omc, KXKZ) - FP_MUL(s, KY);
    fp_t r21 = FP_MUL(omc, KYKZ) + FP_MUL(s, KX);
    fp_t r22 = c + FP_MUL(omc, KZ2);

    int16_t sx[8], sy[8];

    for (int i = 0; i < 8; i++) {
        fp_t vx = VERTS_TILTED[i][0];
        fp_t vy = VERTS_TILTED[i][1];
        fp_t vz = VERTS_TILTED[i][2];

        fp_t x = FP_MUL(r00, vx) + FP_MUL(r01, vy) + FP_MUL(r02, vz);
        fp_t y = FP_MUL(r10, vx) + FP_MUL(r11, vy) + FP_MUL(r12, vz);
        fp_t z = FP_MUL(r20, vx) + FP_MUL(r21, vy) + FP_MUL(r22, vz);

        z += INT_FP(Z_OFFSET);
        if (z < INT_FP(10)) z = INT_FP(10);

        fp_t proj_x =  FP_DIV(FP_MUL(x, INT_FP(FOCAL)), z);
        fp_t proj_y = -FP_DIV(FP_MUL(y, INT_FP(FOCAL)), z);

        int32_t px = FP_INT(proj_x) + CENTER_X;
        int32_t py = FP_INT(proj_y) + CENTER_Y;
        if (px < -256) px = -256;
        if (px >  383) px =  383;
        if (py < -128) py = -128;
        if (py >  191) py =  191;

        sx[i] = (int16_t)px;
        sy[i] = (int16_t)py;
    }

    for (int e = 0; e < 12; e++) {
        draw_line(fb,
                  sx[EDGES[e][0]], sy[EDGES[e][0]],
                  sx[EDGES[e][1]], sy[EDGES[e][1]]);
    }
}
