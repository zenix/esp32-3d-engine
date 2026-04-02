#pragma once
#include "engine3d.h"

// ── Built-in game meshes ──────────────────────────────────────────────────────
// All data is const and lives in flash. Scale by adjusting vertex values or
// the transform_t.z depth field (larger z = smaller on screen).
// All meshes are centred at the origin and sized for a comfortable on-screen
// appearance at z ≈ 180.

// Wireframe cube ±25 units.  8 verts, 12 edges, 12 tri-faces (backface culling enabled).
extern const mesh_t MESH_CUBE;

// Arrow/wedge shape pointing in +Z.  6 verts, 8 edges.
extern const mesh_t MESH_SHIP;

// Irregular 12-faced rock.  12 verts, 18 edges.
extern const mesh_t MESH_ASTEROID;

// Single line segment — used for projectiles / bullets.  2 verts, 1 edge.
extern const mesh_t MESH_BULLET;

// Octahedron (diamond) — pickups, collectibles.  6 verts, 12 edges.
extern const mesh_t MESH_DIAMOND;
