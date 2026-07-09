#ifndef MESH_H
#define MESH_H

#include "core.h"
#include "core_math.h"
#include "core_string.h"
#include "memory_arena.h"

#define MESH_INVALID_HANDLE U32_MAX

typedef u32 mesh_handle_t;

typedef enum
{
    PREDEFINED_MESH_SIMPLE_TRIANGLE   = 0,
    PREDEFINED_MESH_SIMPLE_QUAD       = 1,
    PREDEFINED_MESH_SIMPLE_CUBE       = 2,

    PREDEFINED_MESH_NORMALED_TRIANGLE = 3,
    PREDEFINED_MESH_NORMALED_QUAD     = 4,
    PREDEFINED_MESH_NORMALED_CUBE     = 5,

    PREDEFINED_MESH_COLORED_TRIANGLE  = 6,
    PREDEFINED_MESH_COLORED_QUAD      = 7,
    PREDEFINED_MESH_COLORED_CUBE      = 8,

    PREDEFINED_MESH_TEXTURED_TRIANGLE = 9,
    PREDEFINED_MESH_TEXTURED_QUAD     = 10,
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

mesh_handle_t MeshManager_LoadMesh(string path);

#endif
