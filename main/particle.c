#include "particle.h"
#include "engine3d.h"
#include "esp_timer.h"  // for seeding the PRNG

#define FOCAL    150
#define CENTER_X  64
#define CENTER_Y  32

typedef struct {
    fp_t    x, y, z;
    fp_t    vx, vy, vz;
    uint8_t life;       // frames remaining; 0 = inactive
} particle_t;

static particle_t s_particles[MAX_PARTICLES];

// ── XOR-shift PRNG (32-bit) ───────────────────────────────────────────────────
static uint32_t s_rand_state;

static uint32_t prng_next(void)
{
    uint32_t x = s_rand_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_rand_state = x;
    return x;
}

// Signed random in range [-range, +range] (Q16.16).
static fp_t rand_vel(fp_t range)
{
    // Map 0..UINT32_MAX → -range..+range.
    uint32_t r = prng_next();
    fp_t v = (fp_t)(r % (uint32_t)(2 * range + 1)) - range;
    return v;
}

// ── Public API ────────────────────────────────────────────────────────────────
void particle_init(void)
{
    // Seed PRNG from the microsecond timer for variety.
    s_rand_state = (uint32_t)esp_timer_get_time();
    if (s_rand_state == 0) s_rand_state = 0xDEADBEEFu;
    for (int i = 0; i < MAX_PARTICLES; i++)
        s_particles[i].life = 0;
}

void particle_spawn_burst(fp_t x, fp_t y, fp_t z, int count)
{
    if (s_rand_state == 0) {
        s_rand_state = (uint32_t)esp_timer_get_time();
        if (s_rand_state == 0) s_rand_state = 1;
    }

    for (int i = 0; i < MAX_PARTICLES && count > 0; i++) {
        if (s_particles[i].life != 0) continue;
        particle_t *p = &s_particles[i];
        p->x    = x;
        p->y    = y;
        p->z    = z;
        p->vx   = rand_vel(INT_FP(6));
        p->vy   = rand_vel(INT_FP(6));
        p->vz   = rand_vel(INT_FP(4));
        p->life = 20 + (uint8_t)(prng_next() % 12); // 20-31 frames
        count--;
    }
}

void particle_update(void)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        particle_t *p = &s_particles[i];
        if (p->life == 0) continue;
        p->x += p->vx;
        p->y += p->vy;
        p->z += p->vz;
        p->life--;
    }
}

void particle_draw(uint8_t fb[8][128], int16_t cam_x, int16_t cam_y, int16_t cam_z)
{
    fp_t fcam_x = INT_FP(cam_x);
    fp_t fcam_y = INT_FP(cam_y);
    fp_t fcam_z = INT_FP(cam_z);

    for (int i = 0; i < MAX_PARTICLES; i++) {
        particle_t *p = &s_particles[i];
        if (p->life == 0) continue;

        fp_t x = p->x - fcam_x;
        fp_t y = p->y - fcam_y;
        fp_t z = p->z - fcam_z;

        if (z < INT_FP(10)) continue; // behind camera

        fp_t proj_x =  FP_DIV(FP_MUL(x, INT_FP(FOCAL)), z);
        fp_t proj_y = -FP_DIV(FP_MUL(y, INT_FP(FOCAL)), z);

        int sx = FP_INT(proj_x) + CENTER_X;
        int sy = FP_INT(proj_y) + CENTER_Y;

        engine3d_draw_pixel(fb, sx, sy);
    }
}
