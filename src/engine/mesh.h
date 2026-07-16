#ifndef MESH_H
#define MESH_H

#include "core.h"
#include "core_math.h"
#include "core_string.h"
#include "memory_arena.h"

#define MESH_INVALID_HANDLE NULL

/* opaque: mesh_t is only completed by the engine-private mesh_internal.h,
   so the mesh data (gpu buffer handles) never leaves the engine */
typedef struct _mesh_t mesh_t;
typedef const mesh_t *mesh_handle_t;

typedef enum
{
    PREDEFINED_MESH_SIMPLE_TRIANGLE   = 1,
    PREDEFINED_MESH_SIMPLE_QUAD       = 2,
    PREDEFINED_MESH_SIMPLE_CUBE       = 3,

    PREDEFINED_MESH_NORMALED_TRIANGLE = 4,
    PREDEFINED_MESH_NORMALED_QUAD     = 5,
    PREDEFINED_MESH_NORMALED_CUBE     = 6,

    PREDEFINED_MESH_COLORED_TRIANGLE  = 7,
    PREDEFINED_MESH_COLORED_QUAD      = 8,
    PREDEFINED_MESH_COLORED_CUBE      = 9,

    PREDEFINED_MESH_TEXTURED_TRIANGLE = 10,
    PREDEFINED_MESH_TEXTURED_QUAD     = 11,

    PREDEFINED_MESH_COUNT,
} predefined_mesh_t;

typedef struct _simple_vertex_t simple_vertex_t;
typedef struct _normal_vertex_t normal_vertex_t;
typedef struct _colored_vertex_t colored_vertex_t;
typedef struct _textured_vertex_t textured_vertex_t;

struct _simple_vertex_t
{
    vec3 position;
};

struct _normal_vertex_t
{
    vec3 position;
    vec3 normal;
};

struct _colored_vertex_t
{
    vec3 position;
    vec3 color;
};

struct _textured_vertex_t
{
    vec3 position;
    vec2 texture_coord;
};

bool MeshManager_Init(arena_t *arena);

mesh_handle_t MeshManager_GetPredefinedMesh(predefined_mesh_t mesh);
mesh_handle_t MeshManager_LoadMesh(string path);

#endif
