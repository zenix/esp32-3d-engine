#include "engine3d.h"
#include "fixed_math.h"
#include <stdlib.h>     // abs()
#include <stdbool.h>

// ── Limits ────────────────────────────────────────────────────────────────────
#define ENGINE3D_MAX_VERTS  64      // max vertices supported per mesh

// ── Projection / clipping constants ──────────────────────────────────────────
#define FOCAL       150
#define CENTER_X     64
#define CENTER_Y     32
#define NEAR_PLANE   10             // view-space z below this is behind camera

// ── Rodrigues axis tables ─────────────────────────────────────────────────────
// Four axis presets indexed by transform_t.axis (AXIS_DEFAULT=0 … AXIS_Z=3).
// Each row: { kx, ky, kz, kx2, ky2, kz2, kxky, kxkz, kykz } all Q16.16.
//
// AXIS_DEFAULT: k = normalize([3,2,1]), same constants as the original engine.
// AXIS_X:       k = [1,0,0]
// AXIS_Y:       k = [0,1,0]
// AXIS_Z:       k = [0,0,1]
//
// For pure-axis cases kx²=FP_ONE, all cross terms = 0, etc.
static const fp_t k_axes[4][9] = {
    // kx      ky      kz      kx2    ky2    kz2   kxky  kxkz  kykz
    { 52553,  35035,  17518,  42132, 18726,  4681, 28088, 14044,  9363 }, // DEFAULT
    { 65536,      0,      0,  65536,     0,     0,     0,     0,     0 }, // X
    {     0,  65536,      0,      0, 65536,     0,     0,     0,     0 }, // Y
    {     0,      0,  65536,      0,     0, 65536,     0,     0,     0 }, // Z
};

// ── Pre-tilt: constant Rx(45°) applied to every vertex ────────────────────────
// sin(45°) = cos(45°) = 1/√2 ≈ 0.70711 → Q16.16: 46341
#define BASE_SC  46341

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

    // ── Task 2: select rotation axis ─────────────────────────────────────────
    uint8_t ai = (t->axis < 4) ? t->axis : 0;
    fp_t kx   = k_axes[ai][0], ky   = k_axes[ai][1], kz   = k_axes[ai][2];
    fp_t kx2  = k_axes[ai][3], ky2  = k_axes[ai][4], kz2  = k_axes[ai][5];
    fp_t kxky = k_axes[ai][6], kxkz = k_axes[ai][7], kykz = k_axes[ai][8];

    // Build Rodrigues rotation matrix once per call.
    fp_t s   = fp_sin(t->angle);
    fp_t c   = fp_cos(t->angle);
    fp_t omc = FP_ONE - c;

    fp_t r00 = c + FP_MUL(omc, kx2);
    fp_t r01 = FP_MUL(omc, kxky) - FP_MUL(s, kz);
    fp_t r02 = FP_MUL(omc, kxkz) + FP_MUL(s, ky);
    fp_t r10 = FP_MUL(omc, kxky) + FP_MUL(s, kz);
    fp_t r11 = c + FP_MUL(omc, ky2);
    fp_t r12 = FP_MUL(omc, kykz) - FP_MUL(s, kx);
    fp_t r20 = FP_MUL(omc, kxkz) - FP_MUL(s, ky);
    fp_t r21 = FP_MUL(omc, kykz) + FP_MUL(s, kx);
    fp_t r22 = c + FP_MUL(omc, kz2);

    // World-space translation in Q16.16.
    fp_t tx = INT_FP(t->x);
    fp_t ty = INT_FP(t->y);
    fp_t tz = INT_FP(t->z);

    // ── Task 1: scale factor ──────────────────────────────────────────────────
    // scale==0 → 1.0× (backward compat); 128 → 1.0×; 64 → 0.5×; 192 → 1.5×
    fp_t scale_fp = (t->scale == 0) ? FP_ONE : ((fp_t)t->scale << 9);

    // ── Project all vertices ──────────────────────────────────────────────────
    // We store view-space (x,y,z) for clipping, then screen (sx,sy) after divide.
    static fp_t  vx_vs[ENGINE3D_MAX_VERTS]; // view-space x
    static fp_t  vy_vs[ENGINE3D_MAX_VERTS]; // view-space y
    static fp_t  vz_vs[ENGINE3D_MAX_VERTS]; // view-space z (for near-plane clip)
    static int16_t sx[ENGINE3D_MAX_VERTS];
    static int16_t sy[ENGINE3D_MAX_VERTS];

    // Camera yaw trig computed once (only used if camera active).
    fp_t cyaw = 0, syaw = 0;
    if (g_cam_active) {
        cyaw = fp_cos(g_cam.yaw);
        syaw = fp_sin(g_cam.yaw);
    }

    for (int i = 0; i < mesh->n_verts; i++) {
        // Load from model space and apply scale.
        fp_t vx  = FP_MUL(INT_FP(mesh->verts[i][0]), scale_fp);
        fp_t vy0 = FP_MUL(INT_FP(mesh->verts[i][1]), scale_fp);
        fp_t vz0 = FP_MUL(INT_FP(mesh->verts[i][2]), scale_fp);

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
            fp_t nx =  FP_MUL(cyaw, x) + FP_MUL(syaw, z);
            fp_t nz = -FP_MUL(syaw, x) + FP_MUL(cyaw, z);
            x = nx;
            z = nz;
        }

        // Store view-space coords for near-plane clipping.
        vx_vs[i] = x;
        vy_vs[i] = y;
        vz_vs[i] = z;

        // Perspective project only if in front of near plane.
        // Behind-plane vertices get placeholder screen coords; the edge loop
        // will clip them properly before rasterising.
        if (z >= INT_FP(NEAR_PLANE)) {
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
        // else: sx/sy left uninitialised for this vertex; edge clip handles it.
    }

    // ── Task 4: backface culling ──────────────────────────────────────────────
    // Use view-space 3D normal Z component: nz = (e1 × e2).z
    // where e1 = v1-v0, e2 = v2-v0 in view space (Q16.16).
    // nz ≤ 0 → face normal points away from camera → cull.
    // This is robust to off-screen vertices and near-plane crossings.
    uint8_t face_skip[16] = {0};
    if (mesh->faces && mesh->n_faces > 0) {
        for (int f = 0; f < mesh->n_faces; f++) {
            uint8_t i0 = mesh->faces[f][0];
            uint8_t i1 = mesh->faces[f][1];
            uint8_t i2 = mesh->faces[f][2];
            // View-space edge vectors (Q16.16, shift down to avoid int64 overflow)
            fp_t ax = (vx_vs[i1] - vx_vs[i0]) >> 8;
            fp_t ay = (vy_vs[i1] - vy_vs[i0]) >> 8;
            fp_t bx = (vx_vs[i2] - vx_vs[i0]) >> 8;
            fp_t by = (vy_vs[i2] - vy_vs[i0]) >> 8;
            // Z component of cross product (only component needed for dot with [0,0,1])
            int32_t nz = (int32_t)ax * by - (int32_t)ay * bx;
            if (nz >= 0)
                face_skip[f >> 3] |= (uint8_t)(1u << (f & 7));
        }
    }

    // ── Rasterise edges ───────────────────────────────────────────────────────
    for (int e = 0; e < mesh->n_edges; e++) {
        // Task 4: skip edge if its face is back-facing.
        if (mesh->edge_face) {
            uint8_t fi = mesh->edge_face[e];
            if (fi != 0xFF && (face_skip[fi >> 3] & (1u << (fi & 7))))
                continue;
        }

        uint8_t ia = mesh->edges[e][0];
        uint8_t ib = mesh->edges[e][1];

        bool a_behind = (vz_vs[ia] < INT_FP(NEAR_PLANE));
        bool b_behind = (vz_vs[ib] < INT_FP(NEAR_PLANE));

        // Task 3: near-plane clipping.
        if (a_behind && b_behind) continue; // both behind — skip entirely

        int x0 = sx[ia], y0 = sy[ia];
        int x1 = sx[ib], y1 = sy[ib];

        if (a_behind || b_behind) {
            // Exactly one endpoint is behind the near plane.
            // Parametric clip: find t where z = NEAR_PLANE along the edge.
            fp_t za = vz_vs[ia];
            fp_t zb = vz_vs[ib];
            fp_t dz = zb - za;
            if (dz == 0) continue; // degenerate
            fp_t near_fp = INT_FP(NEAR_PLANE);
            // t = (NEAR - za) / (zb - za)
            fp_t tc = FP_DIV(near_fp - za, dz);
            // Interpolate x and y in view space at the clip point.
            fp_t cx = vx_vs[ia] + FP_MUL(tc, vx_vs[ib] - vx_vs[ia]);
            fp_t cy = vy_vs[ia] + FP_MUL(tc, vy_vs[ib] - vy_vs[ia]);
            // Project the clip point (z = NEAR_PLANE exactly).
            fp_t near_proj = INT_FP(NEAR_PLANE);
            int cpx = FP_INT( FP_DIV(FP_MUL(cx, INT_FP(FOCAL)), near_proj)) + CENTER_X;
            int cpy = FP_INT(-FP_DIV(FP_MUL(cy, INT_FP(FOCAL)), near_proj)) + CENTER_Y;
            if (a_behind) { x0 = cpx; y0 = cpy; }
            else          { x1 = cpx; y1 = cpy; }
        }

        draw_line(fb, x0, y0, x1, y1);
    }
}
