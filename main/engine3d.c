#include "engine3d.h"
#include "fixed_math.h"
#include <stdlib.h>     // abs()
#include <stdbool.h>

// ── Limits ────────────────────────────────────────────────────────────────────
#define ENGINE3D_MAX_VERTS  64      // max vertices supported per mesh

// ── Projection constants ──────────────────────────────────────────────────────
#define FOCAL       150
#define SCREEN_W    128
#define SCREEN_H     64
#define CENTER_X     64
#define CENTER_Y     32

// ── Rotation: Rodrigues axis k = normalize([3, 2, 1]) ─────────────────────────
// All values Q16.16.  kx²+ky²+kz² ≈ FP_ONE (65539, rounding error of 3 ULP).
#define KX    52553   // 3/√14 ≈ 0.80178
#define KY    35035   // 2/√14 ≈ 0.53452
#define KZ    17518   // 1/√14 ≈ 0.26726
#define KX2   42132
#define KY2   18726
#define KZ2    4681
#define KXKY  28088
#define KXKZ  14044
#define KYKZ   9363

// ── Pre-tilt: constant Rx(45°) applied to every vertex ────────────────────────
// Guarantees no face normal can ever reach Z=[0,0,1] during the Rodrigues
// rotation, so the mesh never appears flat/face-on.
// sin(45°) = cos(45°) = 1/√2 ≈ 0.70711 → Q16.16: 46341
#define BASE_SC  46341

// ── Built-in meshes ───────────────────────────────────────────────────────────
static const int8_t _cube_verts[8][3] = {
    {-25,-25,-25}, { 25,-25,-25}, { 25, 25,-25}, {-25, 25,-25},
    {-25,-25, 25}, { 25,-25, 25}, { 25, 25, 25}, {-25, 25, 25},
};
static const uint8_t _cube_edges[12][2] = {
    {0,1},{1,2},{2,3},{3,0},
    {4,5},{5,6},{6,7},{7,4},
    {0,4},{1,5},{2,6},{3,7},
};
const mesh_t MESH_CUBE = {
    .verts   = _cube_verts,
    .edges   = _cube_edges,
    .n_verts = 8,
    .n_edges = 12,
};

// ── Camera state ──────────────────────────────────────────────────────────────
static camera_t  g_cam;
static bool      g_cam_active = false;

void engine3d_set_camera(const camera_t *cam)
{
    if (cam) {
        g_cam        = *cam;
        g_cam_active = true;
    } else {
        g_cam_active = false;
    }
}

// ── Rasterisation ─────────────────────────────────────────────────────────────
// draw_pixel is now a public inline in engine3d.h — call it directly here.

static void draw_line(uint8_t fb[8][128], int x0, int y0, int x1, int y1)
{
    int dx =  abs(x1 - x0);
    int dy =  abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        engine3d_draw_pixel(fb, x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err << 1;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

// ── Draw ──────────────────────────────────────────────────────────────────────
void engine3d_draw_mesh(uint8_t fb[8][128], const mesh_t *mesh, const transform_t *t)
{
    if (mesh->n_verts > ENGINE3D_MAX_VERTS) return;

    // Build Rodrigues rotation matrix once per call.
    fp_t s   = fp_sin(t->angle);
    fp_t c   = fp_cos(t->angle);
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

    // World-space translation in Q16.16.
    fp_t tx = INT_FP(t->x);
    fp_t ty = INT_FP(t->y);
    fp_t tz = INT_FP(t->z);

    // Project all vertices.
    static int16_t sx[ENGINE3D_MAX_VERTS];
    static int16_t sy[ENGINE3D_MAX_VERTS];

    for (int i = 0; i < mesh->n_verts; i++) {
        // Load from model space.
        fp_t vx  = INT_FP(mesh->verts[i][0]);
        fp_t vy0 = INT_FP(mesh->verts[i][1]);
        fp_t vz0 = INT_FP(mesh->verts[i][2]);

        // Rx(45°) pre-tilt: vy' = (vy-vz)/√2,  vz' = (vy+vz)/√2
        fp_t vy = FP_MUL(BASE_SC, vy0) - FP_MUL(BASE_SC, vz0);
        fp_t vz = FP_MUL(BASE_SC, vy0) + FP_MUL(BASE_SC, vz0);

        // Rodrigues rotation.
        fp_t x = FP_MUL(r00, vx) + FP_MUL(r01, vy) + FP_MUL(r02, vz);
        fp_t y = FP_MUL(r10, vx) + FP_MUL(r11, vy) + FP_MUL(r12, vz);
        fp_t z = FP_MUL(r20, vx) + FP_MUL(r21, vy) + FP_MUL(r22, vz);

        // World translation.
        x += tx;
        y += ty;
        z += tz;

        // Camera: inverse-translate then yaw rotation around Y axis.
        if (g_cam_active) {
            x -= INT_FP(g_cam.x);
            y -= INT_FP(g_cam.y);
            z -= INT_FP(g_cam.z);

            // Rotate by -yaw around Y: x' = cos*x + sin*z, z' = -sin*x + cos*z
            fp_t cyaw = fp_cos(g_cam.yaw);
            fp_t syaw = fp_sin(g_cam.yaw);
            fp_t nx =  FP_MUL(cyaw, x) + FP_MUL(syaw, z);
            fp_t nz = -FP_MUL(syaw, x) + FP_MUL(cyaw, z);
            x = nx;
            z = nz;
        }

        if (z < INT_FP(10)) z = INT_FP(10);

        // Perspective projection (Y negated: model Y-up → screen Y-down).
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

    // Rasterise edges.
    for (int e = 0; e < mesh->n_edges; e++) {
        draw_line(fb,
                  sx[mesh->edges[e][0]], sy[mesh->edges[e][0]],
                  sx[mesh->edges[e][1]], sy[mesh->edges[e][1]]);
    }
}
