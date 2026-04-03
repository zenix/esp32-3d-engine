#include "doom.h"
#include "raycast.h"
#include "billboard.h"
#include "pattern.h"
#include "engine3d.h"
#include "fixed_math.h"
#include "input.h"
#include "sound.h"
#include "font.h"
#include "scene.h"
#include "game.h"
#include "meshes.h"
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

// ── Tuning constants ──────────────────────────────────────────────────────────
#define FOCAL       150         // must match raycast.c and engine3d.c
#define CENTER_X     64
#define CENTER_Y     32
#define NEAR_PLANE   10
#define TILE_SHIFT    6         // tile size = 64 world units
#define TILE_SZ      (1 << TILE_SHIFT)
#define MAP_W        20
#define MAP_H        20

#define PLAYER_TURN     1       // angle units per frame
#define PLAYER_SPEED    3       // world units per frame (forward/back)
#define PLAYER_MARGIN   10      // collision margin in world units

#define PLAYER_MAX_HP   100
#define PLAYER_MAX_AMMO  50
#define PLAYER_START_AMMO 20

#define ENEMY_SPEED      1      // world units per frame
#define ENEMY_HEALTH     3      // hits to kill
#define ENEMY_ATK_DIST   100    // attack when within this many world units (sq root)
#define ENEMY_ATK_DIST_SQ (ENEMY_ATK_DIST * ENEMY_ATK_DIST)
#define ENEMY_CHASE_DIST  480   // start chasing (world units)
#define ENEMY_ATK_PERIOD  60    // frames between attacks
#define ENEMY_ATK_DMGMIN  8
#define ENEMY_ATK_DMGMAX  15

#define PICKUP_RADIUS_SQ (28 * 28)
#define HEALTH_BONUS     25
#define AMMO_BONUS       10

#define GUN_CX  64
#define GUN_CY  57

// ── Level data ────────────────────────────────────────────────────────────────
// 20×20 tile map. Row-major: cells[z * 20 + x].
// Values: 0=empty, 1=MAP_WALL_1 (solid), 2=MAP_WALL_2 (checkerboard)
static const uint8_t lvl1_cells[MAP_W * MAP_H] = {
    // z=0
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    // z=1  (player start at tile 2,1)
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    // z=2
    1,0,1,1,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,1,
    // z=3
    1,0,1,0,0,0,0,0,0,0,0,0,0,1,0,1,0,0,0,1,
    // z=4
    1,0,1,1,0,0,0,0,0,0,0,0,0,1,0,1,0,0,0,1,
    // z=5
    1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,1,
    // z=6  (start of checkered room)
    1,0,0,0,2,2,2,2,2,2,2,2,2,0,0,0,0,0,0,1,
    // z=7
    1,0,0,2,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,1,
    // z=8  (pickups and enemy inside room)
    1,0,0,2,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,1,
    // z=9
    1,0,0,2,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,1,
    // z=10
    1,0,0,0,2,2,2,2,2,2,2,2,2,0,0,0,0,0,0,1,
    // z=11
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    // z=12 — dividing wall with gap at x=9,10
    1,1,1,1,1,1,1,1,1,0,0,1,1,1,1,1,1,1,1,1,
    // z=13
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    // z=14
    1,0,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,
    // z=15
    1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,
    // z=16
    1,0,1,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,
    // z=17
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    // z=18
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    // z=19
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
};

// Enemy spawn positions (tile coords, 0xFF = end)
static const uint8_t lvl1_etx[] = {4,  14,  8,  5, 10,  7, 0xFF};
static const uint8_t lvl1_etz[] = {3,   3,  8, 14, 15, 17, 0xFF};

// Pickup spawn positions (tile coords + type: 0=health, 1=ammo, 0xFF = end)
static const uint8_t lvl1_ptx[]  = {6,  10,  5, 14, 0xFF};
static const uint8_t lvl1_ptz[]  = {8,   8, 17, 17, 0xFF};
static const uint8_t lvl1_ptyp[] = {0,   1,  0,  1, 0xFF};

// ── Player state ──────────────────────────────────────────────────────────────
static fp_t     s_px, s_pz;
static uint8_t  s_yaw;
static int16_t  s_health;
static int8_t   s_ammo;
static uint8_t  s_weapon_frame;   // 0=idle, 1=fired (recoil), 2=return
static uint8_t  s_weapon_timer;
static uint8_t  s_damage_flash;
static uint16_t s_score;

// ── Map ───────────────────────────────────────────────────────────────────────
static uint8_t      s_cells[MAP_W * MAP_H];
static raycast_map_t s_map;

// ── Enemies ───────────────────────────────────────────────────────────────────
#define MAX_ENEMIES 6
#define ESTATE_IDLE   0
#define ESTATE_CHASE  1
#define ESTATE_ATTACK 2
#define ESTATE_DYING  3

typedef struct {
    fp_t    x, z;
    int8_t  health;
    uint8_t state;
    uint8_t timer;
    bool    active;
} enemy_t;
static enemy_t s_enemies[MAX_ENEMIES];

// ── Pickups ───────────────────────────────────────────────────────────────────
#define MAX_PICKUPS 4
#define PU_HEALTH 0
#define PU_AMMO   1

typedef struct {
    fp_t    x, z;
    uint8_t type;
    bool    active;
} pickup_t;
static pickup_t s_pickups[MAX_PICKUPS];

// ── Helpers ───────────────────────────────────────────────────────────────────

static uint8_t cell_at(int tx, int tz) {
    if ((unsigned)tx >= MAP_W || (unsigned)tz >= MAP_H) return 1;
    return s_cells[tz * MAP_W + tx];
}

// Move player by (mvx, mvz) with tile-aligned sliding collision.
static void move_player(fp_t mvx, fp_t mvz) {
    fp_t nx = s_px + mvx;
    fp_t nz = s_pz + mvz;
    // Check corner points (player occupies a small circle approximated by 4 AABB corners)
    int m = PLAYER_MARGIN;
    // Try full move
    int tx = FP_INT(nx) >> TILE_SHIFT;
    int tz = FP_INT(nz) >> TILE_SHIFT;
    bool blocked_x = cell_at((FP_INT(nx)+m) >> TILE_SHIFT, tz)
                  || cell_at((FP_INT(nx)-m) >> TILE_SHIFT, tz);
    bool blocked_z = cell_at(tx, (FP_INT(nz)+m) >> TILE_SHIFT)
                  || cell_at(tx, (FP_INT(nz)-m) >> TILE_SHIFT);
    if (!blocked_x) s_px = nx;
    if (!blocked_z) s_pz = nz;
}

// Simplified LOS check: march from (ax,az) toward (bx,bz) in integer steps.
static bool check_los(fp_t ax, fp_t az, fp_t bx, fp_t bz) {
    int ax_i = FP_INT(ax), az_i = FP_INT(az);
    int bx_i = FP_INT(bx), bz_i = FP_INT(bz);
    int dx = bx_i - ax_i, dz = bz_i - az_i;
    int adx = dx < 0 ? -dx : dx, adz = dz < 0 ? -dz : dz;
    int len = adx > adz ? adx : adz;
    int step = TILE_SZ / 2;
    if (len == 0) return true;
    for (int d = step; d < len; d += step) {
        int rx = ax_i + dx * d / len;
        int rz = az_i + dz * d / len;
        int tx = rx >> TILE_SHIFT;
        int tz = rz >> TILE_SHIFT;
        if ((unsigned)tx >= MAP_W || (unsigned)tz >= MAP_H) return false;
        if (s_cells[tz * MAP_W + tx] != MAP_EMPTY) return false;
        if (tx == (bx_i >> TILE_SHIFT) && tz == (bz_i >> TILE_SHIFT)) return true;
    }
    return true;
}

// Bresenham line draw (used for weapon overlay).
static void doom_line(uint8_t fb[8][128], int x0, int y0, int x1, int y1) {
    int dx = x1 - x0, dy = y1 - y0;
    int sx = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
    int sy = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    int err = (dx > dy ? dx : -dy) / 2;
    for (;;) {
        engine3d_draw_pixel(fb, x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 <  dy) { err += dx; y0 += sy; }
    }
}

// ── Weapon overlay ────────────────────────────────────────────────────────────
// Line segments { x0,y0,x1,y1 } relative to (GUN_CX, GUN_CY).
typedef struct { int8_t x0, y0, x1, y1; } gun_line_t;

static const gun_line_t s_gun_lines[7] = {
    {  -4, -10,  4, -10 },   // barrel tip
    {  -4, -10, -4,  -2 },   // barrel left
    {   4, -10,  4,  -2 },   // barrel right
    {  -7,  -2,  7,  -2 },   // grip top
    {  -7,  -2, -7,   5 },   // grip left
    {   7,  -2,  7,   5 },   // grip right
    {  -7,   5,  7,   5 },   // grip bottom
};

static void draw_weapon(uint8_t fb[8][128]) {
    int y_off = (s_weapon_frame == 1) ? -3 : 0;  // recoil on firing
    for (int i = 0; i < 7; i++) {
        doom_line(fb,
            GUN_CX + s_gun_lines[i].x0, GUN_CY + s_gun_lines[i].y0 + y_off,
            GUN_CX + s_gun_lines[i].x1, GUN_CY + s_gun_lines[i].y1 + y_off);
    }
    // Muzzle flash on fire frame
    if (s_weapon_frame == 1) {
        int flash_y = GUN_CY - 13 + y_off;
        engine3d_draw_pixel(fb, GUN_CX,      flash_y - 2);
        engine3d_draw_pixel(fb, GUN_CX - 3,  flash_y);
        engine3d_draw_pixel(fb, GUN_CX,      flash_y);
        engine3d_draw_pixel(fb, GUN_CX + 3,  flash_y);
        engine3d_draw_pixel(fb, GUN_CX,      flash_y + 2);
    }
}

// ── Shooting ──────────────────────────────────────────────────────────────────

static void do_shoot(void) {
    if (s_ammo <= 0) return;
    s_ammo--;
    sound_play(SFX_SHOOT);
    s_weapon_frame = 1;
    s_weapon_timer = 8;

    fp_t sy = fp_sin(s_yaw);
    fp_t cy = fp_cos(s_yaw);

    // Wall distance at screen centre from last raycast frame
    const ray_hit_t *hits = raycast_get_hits();
    fp_t wall_dist = hits[CENTER_X].dist;

    int hit_idx = -1;
    fp_t hit_dist = wall_dist;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!s_enemies[i].active || s_enemies[i].health <= 0) continue;

        fp_t dx = s_enemies[i].x - s_px;
        fp_t dz = s_enemies[i].z - s_pz;

        // Transform to view space
        fp_t vx = FP_MUL(cy, dx) + FP_MUL(sy, dz);
        fp_t vz = FP_MUL(-sy, dx) + FP_MUL(cy, dz);

        if (FP_INT(vz) < NEAR_PLANE) continue;
        if (vz >= hit_dist) continue;

        // Is enemy near screen centre?  Tolerance scales with sprite half-width
        // so distant (small) enemies are still hittable (minimum ±5 px).
        int sx = FP_INT(FP_DIV(FP_MUL(vx, INT_FP(FOCAL)), vz)) + CENTER_X;
        int half_h = FP_INT(FP_DIV(INT_FP(48 * FOCAL / 2), vz));
        if (half_h > CENTER_Y) half_h = CENTER_Y;
        int tol = half_h / 2;   // sprite screen half-width
        if (tol < 5) tol = 5;
        if (sx < CENTER_X - tol || sx > CENTER_X + tol) continue;

        hit_idx  = i;
        hit_dist = vz;
    }

    if (hit_idx >= 0) {
        s_enemies[hit_idx].health--;
        s_score += 10;
        if (s_enemies[hit_idx].health <= 0) {
            s_enemies[hit_idx].state = ESTATE_DYING;
            s_enemies[hit_idx].timer = 20;
            s_score += 90;
        } else {
            sound_play(SFX_ENEMY_HURT);
            s_enemies[hit_idx].state = ESTATE_CHASE;  // aggro on hit
        }
    }
}

// ── Enemy update ──────────────────────────────────────────────────────────────

static void update_enemies(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemy_t *e = &s_enemies[i];
        if (!e->active) continue;

        int dx_i = FP_INT(s_px - e->x);
        int dz_i = FP_INT(s_pz - e->z);
        int dist_sq = dx_i * dx_i + dz_i * dz_i;

        switch (e->state) {
        case ESTATE_IDLE:
            // Activate when player is close enough and LOS exists
            if (dist_sq < ENEMY_CHASE_DIST * ENEMY_CHASE_DIST) {
                if (check_los(e->x, e->z, s_px, s_pz))
                    e->state = ESTATE_CHASE;
            }
            break;

        case ESTATE_CHASE: {
            if (dist_sq < ENEMY_ATK_DIST_SQ) {
                e->state = ESTATE_ATTACK;
                e->timer = ENEMY_ATK_PERIOD;
                break;
            }
            // Move toward player using L-infinity normalised step
            fp_t dx = s_px - e->x;
            fp_t dz = s_pz - e->z;
            int adx = dx_i < 0 ? -dx_i : dx_i;
            int adz = dz_i < 0 ? -dz_i : dz_i;
            int mx  = adx > adz ? adx : adz;
            if (mx > 0) {
                fp_t mvx = FP_DIV(FP_MUL(dx, INT_FP(ENEMY_SPEED)), INT_FP(mx));
                fp_t mvz = FP_DIV(FP_MUL(dz, INT_FP(ENEMY_SPEED)), INT_FP(mx));
                // Simple tile collision for enemies
                fp_t nx = e->x + mvx, nz = e->z + mvz;
                int tx = FP_INT(nx) >> TILE_SHIFT;
                int tz = FP_INT(nz) >> TILE_SHIFT;
                if (cell_at(tx, FP_INT(e->z) >> TILE_SHIFT) == MAP_EMPTY) e->x += mvx;
                if (cell_at(FP_INT(e->x) >> TILE_SHIFT, tz) == MAP_EMPTY) e->z += mvz;
            }
            // Lose interest if LOS broken and far away
            if (dist_sq > ENEMY_CHASE_DIST * ENEMY_CHASE_DIST * 4
                    && !check_los(e->x, e->z, s_px, s_pz))
                e->state = ESTATE_IDLE;
            break;
        }

        case ESTATE_ATTACK:
            if (dist_sq > ENEMY_ATK_DIST_SQ * 4) {
                e->state = ESTATE_CHASE;
                break;
            }
            if (e->timer > 0) {
                e->timer--;
            } else {
                // Attack!
                // Random damage: use frame counter as a cheap PRNG
                int dmg = ENEMY_ATK_DMGMIN + (int)(s_score & 7);
                if (dmg > ENEMY_ATK_DMGMAX) dmg = ENEMY_ATK_DMGMAX;
                s_health -= (int16_t)dmg;
                s_damage_flash = 8;
                sound_play(SFX_PLAYER_HURT);
                e->timer = ENEMY_ATK_PERIOD;
            }
            break;

        case ESTATE_DYING:
            if (e->timer > 0) {
                e->timer--;
            } else {
                e->active = false;
            }
            break;
        }
    }
}

// ── Pickup update ─────────────────────────────────────────────────────────────

static void update_pickups(void) {
    for (int i = 0; i < MAX_PICKUPS; i++) {
        pickup_t *p = &s_pickups[i];
        if (!p->active) continue;
        int dx = FP_INT(s_px - p->x);
        int dz = FP_INT(s_pz - p->z);
        if (dx * dx + dz * dz < PICKUP_RADIUS_SQ) {
            if (p->type == PU_HEALTH) {
                s_health += HEALTH_BONUS;
                if (s_health > PLAYER_MAX_HP) s_health = PLAYER_MAX_HP;
            } else {
                s_ammo += AMMO_BONUS;
                if (s_ammo > PLAYER_MAX_AMMO) s_ammo = PLAYER_MAX_AMMO;
            }
            sound_play(SFX_PICKUP);
            p->active = false;
        }
    }
}

// ── Sprite sort & draw ────────────────────────────────────────────────────────

typedef struct { fp_t dist; uint8_t spr_id; fp_t wx, wz; } sprite_ref_t;

static void draw_sprites(uint8_t fb[8][128]) {
    sprite_ref_t refs[MAX_ENEMIES + MAX_PICKUPS];
    int n = 0;
    fp_t sy = fp_sin(s_yaw);
    fp_t cy = fp_cos(s_yaw);

    // Collect visible sprites with their distances
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemy_t *e = &s_enemies[i];
        if (!e->active) continue;
        fp_t dx = e->x - s_px, dz = e->z - s_pz;
        fp_t vz = FP_MUL(-sy, dx) + FP_MUL(cy, dz);
        if (FP_INT(vz) < NEAR_PLANE) continue;
        uint8_t spr = (e->state == ESTATE_ATTACK) ? SPR_ENEMY_ATCK : SPR_ENEMY_IDLE;
        refs[n++] = (sprite_ref_t){ vz, spr, e->x, e->z };
    }
    for (int i = 0; i < MAX_PICKUPS; i++) {
        pickup_t *p = &s_pickups[i];
        if (!p->active) continue;
        fp_t dx = p->x - s_px, dz = p->z - s_pz;
        fp_t vz = FP_MUL(-sy, dx) + FP_MUL(cy, dz);
        if (FP_INT(vz) < NEAR_PLANE) continue;
        uint8_t spr = (p->type == PU_HEALTH) ? SPR_HEALTH : SPR_AMMO;
        refs[n++] = (sprite_ref_t){ vz, spr, p->x, p->z };
    }

    // Insertion sort by descending distance (back-to-front painter's order)
    for (int i = 1; i < n; i++) {
        sprite_ref_t tmp = refs[i];
        int j = i - 1;
        while (j >= 0 && refs[j].dist < tmp.dist) {
            refs[j + 1] = refs[j];
            j--;
        }
        refs[j + 1] = tmp;
    }

    for (int i = 0; i < n; i++) {
        billboard_draw(fb, refs[i].wx, refs[i].wz, s_px, s_pz, s_yaw, refs[i].spr_id);
    }
}

// ── HUD ───────────────────────────────────────────────────────────────────────

static void draw_hud(uint8_t fb[8][128]) {
    // Top bar: HP and ammo
    font_draw_string(fb,  0, 0, "HP");
    font_draw_int(fb,    12, 0, s_health);
    font_draw_string(fb, 66, 0, "AM");
    font_draw_int(fb,    78, 0, s_ammo);

    // Crosshair
    engine3d_draw_pixel(fb, CENTER_X,     CENTER_Y);
    engine3d_draw_pixel(fb, CENTER_X - 2, CENTER_Y);
    engine3d_draw_pixel(fb, CENTER_X + 2, CENTER_Y);
    engine3d_draw_pixel(fb, CENTER_X,     CENTER_Y - 2);
    engine3d_draw_pixel(fb, CENTER_X,     CENTER_Y + 2);
}

// ── Scene: PLAY ───────────────────────────────────────────────────────────────

static void play_enter(game_t *g) {
    // Copy level cells to mutable buffer (game can open doors by writing 0)
    memcpy(s_cells, lvl1_cells, MAP_W * MAP_H);
    s_map = (raycast_map_t){ s_cells, MAP_W, MAP_H, TILE_SHIFT };

    // Spawn enemies
    memset(s_enemies, 0, sizeof(s_enemies));
    for (int i = 0; i < MAX_ENEMIES && lvl1_etx[i] != 0xFF; i++) {
        s_enemies[i].x      = INT_FP(lvl1_etx[i] * TILE_SZ + TILE_SZ / 2);
        s_enemies[i].z      = INT_FP(lvl1_etz[i] * TILE_SZ + TILE_SZ / 2);
        s_enemies[i].health = ENEMY_HEALTH;
        s_enemies[i].state  = ESTATE_IDLE;
        s_enemies[i].active = true;
    }

    // Spawn pickups
    memset(s_pickups, 0, sizeof(s_pickups));
    for (int i = 0; i < MAX_PICKUPS && lvl1_ptx[i] != 0xFF; i++) {
        s_pickups[i].x    = INT_FP(lvl1_ptx[i] * TILE_SZ + TILE_SZ / 2);
        s_pickups[i].z    = INT_FP(lvl1_ptz[i] * TILE_SZ + TILE_SZ / 2);
        s_pickups[i].type = lvl1_ptyp[i];
        s_pickups[i].active = true;
    }

    // Player
    s_px           = INT_FP(2 * TILE_SZ + TILE_SZ / 2);
    s_pz           = INT_FP(1 * TILE_SZ + TILE_SZ / 2);
    s_yaw          = 0;
    s_health       = PLAYER_MAX_HP;
    s_ammo         = PLAYER_START_AMMO;
    s_weapon_frame = 0;
    s_weapon_timer = 0;
    s_damage_flash = 0;
    s_score        = 0;
}

static void play_update(game_t *g, fp_t dt) {
    // ── Input ─────────────────────────────────────────────────────────────────
    if (input_held(BTN_LEFT))  s_yaw = (uint8_t)(s_yaw + PLAYER_TURN);
    if (input_held(BTN_RIGHT)) s_yaw = (uint8_t)(s_yaw - PLAYER_TURN);

    if (input_held(BTN_UP)) {
        move_player(
            FP_MUL(-fp_sin(s_yaw), INT_FP(PLAYER_SPEED)),
            FP_MUL( fp_cos(s_yaw), INT_FP(PLAYER_SPEED)));
    }
    if (input_held(BTN_DOWN)) {
        move_player(
            FP_MUL( fp_sin(s_yaw), INT_FP(PLAYER_SPEED)),
            FP_MUL(-fp_cos(s_yaw), INT_FP(PLAYER_SPEED)));
    }

    if (input_just_pressed(BTN_ACTION)) do_shoot();

    // ── Weapon animation ──────────────────────────────────────────────────────
    if (s_weapon_timer > 0) {
        s_weapon_timer--;
        if (s_weapon_timer == 0) {
            s_weapon_frame = (s_weapon_frame > 0) ? s_weapon_frame - 1 : 0;
            if (s_weapon_frame > 0) s_weapon_timer = 4;
        }
    }

    // ── Game logic ────────────────────────────────────────────────────────────
    update_enemies();
    update_pickups();

    if (s_damage_flash > 0) s_damage_flash--;

    // ── Death check ───────────────────────────────────────────────────────────
    if (s_health <= 0) {
        sound_play(SFX_GAMEOVER);
        game_switch_scene(g, &SCENE_DOOM_OVER);
    }
}

static void play_render(game_t *g, uint8_t fb[8][128]) {
    raycast_render(fb, &s_map, s_px, s_pz, s_yaw);
    draw_sprites(fb);
    draw_weapon(fb);
    draw_hud(fb);

    // Damage flash: draw screen border for a few frames
    if (s_damage_flash > 0) {
        for (int x = 0; x < 128; x++) {
            engine3d_draw_pixel(fb, x, 0);
            engine3d_draw_pixel(fb, x, 6);
            engine3d_draw_pixel(fb, x, 57);
            engine3d_draw_pixel(fb, x, 63);
        }
        for (int y = 0; y < 64; y++) {
            engine3d_draw_pixel(fb, 0,   y);
            engine3d_draw_pixel(fb, 1,   y);
            engine3d_draw_pixel(fb, 126, y);
            engine3d_draw_pixel(fb, 127, y);
        }
    }
}

// ── Scene: TITLE ─────────────────────────────────────────────────────────────

static void title_update(game_t *g, fp_t dt) {
    if (input_just_pressed(BTN_ACTION))
        game_switch_scene(g, &SCENE_DOOM_PLAY);
}

static void title_render(game_t *g, uint8_t fb[8][128]) {
    // Spinning skull/asteroid background mesh
    transform_t t = {
        .x     = 0,
        .y     = 2,
        .z     = 180,
        .angle = (uint8_t)(g->frame_count * 2),
        .axis  = AXIS_DEFAULT,
        .scale = 128,
    };
    engine3d_set_camera(NULL);
    engine3d_draw_mesh(fb, &MESH_ASTEROID, &t);

    font_draw_string(fb, 52, 6,  "DOOM");
    font_draw_string(fb, 51, 6,  "DOOM");  // bold pass

    font_draw_string(fb, 16, 20, "CORRIDOR CRAWLER");
    font_draw_string(fb, 28, 40, "ACTION: START");
}

// ── Scene: GAME OVER ─────────────────────────────────────────────────────────

static void over_update(game_t *g, fp_t dt) {
    if (input_just_pressed(BTN_ACTION))
        game_switch_scene(g, &SCENE_DOOM_PLAY);
}

static void over_render(game_t *g, uint8_t fb[8][128]) {
    font_draw_string(fb, 37, 10, "YOU DIED");
    font_draw_string(fb, 22, 24, "SCORE:");
    font_draw_int(fb,    58, 24, s_score);
    font_draw_string(fb, 16, 40, "ACTION: RETRY");
}

// ── Scene definitions ─────────────────────────────────────────────────────────

const scene_t SCENE_DOOM_TITLE = {
    .on_enter = NULL,
    .on_exit  = NULL,
    .update   = title_update,
    .render   = title_render,
};

const scene_t SCENE_DOOM_PLAY = {
    .on_enter = play_enter,
    .on_exit  = NULL,
    .update   = play_update,
    .render   = play_render,
};

const scene_t SCENE_DOOM_OVER = {
    .on_enter = NULL,
    .on_exit  = NULL,
    .update   = over_update,
    .render   = over_render,
};
