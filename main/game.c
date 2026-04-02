#include "game.h"
#include <string.h>  // memset

// ── Game init ─────────────────────────────────────────────────────────────────
void game_init(game_t *g)
{
    memset(g, 0, sizeof(*g));
}

void game_switch_scene(game_t *g, const scene_t *next)
{
    scene_exit(g->current_scene, g);
    g->current_scene = next;
    scene_enter(g->current_scene, g);
}

// ── Entity management ─────────────────────────────────────────────────────────
entity_t *entity_spawn(game_t *g, const mesh_t *mesh, uint8_t type)
{
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (!g->entities[i].active) {
            entity_t *e = &g->entities[i];
            memset(e, 0, sizeof(*e));
            e->mesh            = mesh;
            e->type            = type;
            e->active          = true;
            e->transform.z     = 180; // sensible default depth
            e->fz              = INT_FP(180);
            e->collision_radius = INT_FP(20); // sensible default radius
            return e;
        }
    }
    return NULL; // no free slots
}

void entity_kill(entity_t *e)
{
    if (e) e->active = false;
}

void entity_update_positions(game_t *g)
{
    for (int i = 0; i < MAX_ENTITIES; i++) {
        entity_t *e = &g->entities[i];
        if (!e->active) continue;
        e->fx += e->vx;
        e->fy += e->vy;
        e->fz += e->vz;
        // Sync integer transform used by the renderer.
        e->transform.x = (int16_t)FP_INT(e->fx);
        e->transform.y = (int16_t)FP_INT(e->fy);
        e->transform.z = (int16_t)FP_INT(e->fz);
        // Keep depth positive to avoid divide-by-zero in projection.
        if (e->transform.z < 10) e->transform.z = 10;
    }
}

void entity_draw_all(game_t *g, uint8_t fb[8][128])
{
    if (g->camera_active)
        engine3d_set_camera(&g->camera);
    else
        engine3d_set_camera(NULL);

    // Task 5: depth-sort — build index array, insertion-sort by descending fz
    // (farther entities drawn first so closer ones overdraw them correctly).
    // Camera-relative Z: subtract camera.z when camera is active.
    uint8_t order[MAX_ENTITIES];
    int n = 0;
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (g->entities[i].active && g->entities[i].mesh)
            order[n++] = (uint8_t)i;
    }
    fp_t cam_fz = g->camera_active ? INT_FP(g->camera.z) : 0;
    for (int i = 1; i < n; i++) {
        uint8_t key = order[i];
        fp_t key_z  = g->entities[key].fz - cam_fz;
        int j = i - 1;
        while (j >= 0 && (g->entities[order[j]].fz - cam_fz) < key_z) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }
    for (int i = 0; i < n; i++)
        engine3d_draw_mesh(fb, g->entities[order[i]].mesh, &g->entities[order[i]].transform);
}

