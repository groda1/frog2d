#include <stdio.h>
#include <stdlib.h>

#include "core.h"
#include "file.h"
#include "hash_map.h"
#include "log.h"
#include "memory_arena.h"
#include "os_path.h"
#include "os_time.h"

#include "mesh.h"
#include "mesh_internal.h"
#include "renderer.h"
#include "vulkan_renderer.h"

#define MAX_RESOURCE_PATH 512
#define OBJ_MAX_LINE      512

/* obj indices are packed 21 bits each into a u64 dedup key */
#define OBJ_MAX_INDEX ((1u << 21) - 1)

typedef struct
{
    /* 1-based obj indices, 0 = not present */
    u32 v[3];
    u32 t[3];
    u32 n[3];
} obj_face_t;

typedef struct
{
    const mesh_t *predefined[PREDEFINED_MESH_COUNT];
} mesh_collection_t;

static mesh_collection_t *s_meshes;

extern arena_t *g_engine_arena;

static void obj_next_line(const char **cursor, const char *end, char *buf, u64 buf_size);
static bool obj_parse_floats(const char *str, f32 *out, u32 count);
static bool obj_parse_face(const char *str, obj_face_t *face);
static bool load_obj_mesh(string path, mesh_t *mesh_out);
static mesh_t *create_mesh(VkBuffer vertex_buffer, VkBuffer index_buffer, u32 index_count);
static void load_predefined_meshes(void);

bool MeshManager_Init()
{
    Assert(g_engine_arena != NULL);

    s_meshes = arena_push(g_engine_arena, mesh_collection_t);

    load_predefined_meshes();

    Log(INFO, "Mesh manager initialized");
    return true;
}

mesh_handle_t MeshManager_GetPredefinedMesh(predefined_mesh_t mesh)
{
    Assert(mesh > 0 && mesh < PREDEFINED_MESH_COUNT);

    const mesh_t *predefined = s_meshes->predefined[mesh];
    AssertAlways(predefined != NULL);

    return predefined;
}

mesh_handle_t MeshManager_LoadMesh(string path) // TODO CANNOT BE STRING
{
    string extension = NullString;
    for (u64 i = path.len; i > 0; i--)
    {
        if (path.str[i - 1] == '.')
        {
            extension = string_from_l((const char *)path.str + i, path.len - i);
            break;
        }
    }

    if (extension.len == 0)
    {
        Log(ERROR, "unknown file type: %S", path);
        return MESH_INVALID_HANDLE;
    }

    if (string_match(extension, string_lit("obj")))
    {
        mesh_t mesh;
        if (!load_obj_mesh(path, &mesh))
            return MESH_INVALID_HANDLE;

        return create_mesh(mesh.vertex_buffer, mesh.index_buffer, mesh.index_count);
    }

    Log(ERROR, "failed to load mesh: %S", path);
    return MESH_INVALID_HANDLE;
}


/* copies the line at *cursor into buf (null-terminated, CR stripped, truncated
   to buf_size) and advances *cursor past the newline */
static void obj_next_line(const char **cursor, const char *end, char *buf, u64 buf_size)
{
    const char *ptr = *cursor;
    u64 len = 0;
    while (ptr < end && *ptr != '\n')
    {
        if (len + 1 < buf_size && *ptr != '\r')
            buf[len++] = (char)*ptr;
        ptr++;
    }
    buf[len] = '\0';
    *cursor = (ptr < end) ? ptr + 1 : end;
}

static bool obj_parse_floats(const char *str, f32 *out, u32 count)
{
    const char *cursor = str;
    for (u32 i = 0; i < count; i++)
    {
        char *num_end;
        out[i] = strtof(cursor, &num_end);
        if (num_end == cursor)
            return false;
        cursor = num_end;
    }
    return true;
}

/* accepts triangles with refs of the forms v, v/t, v//n and v/t/n */
static bool obj_parse_face(const char *str, obj_face_t *face)
{
    const char *cursor = str;
    for (u32 i = 0; i < 3; i++)
    {
        char *num_end;
        u64 v = strtoul(cursor, &num_end, 10);
        if (num_end == cursor || v == 0 || v > OBJ_MAX_INDEX)
            return false;
        cursor = num_end;

        u64 t = 0;
        u64 n = 0;
        if (*cursor == '/')
        {
            cursor++;
            t = strtoul(cursor, &num_end, 10);
            if (t > OBJ_MAX_INDEX)
                return false;
            cursor = num_end;

            if (*cursor == '/')
            {
                cursor++;
                n = strtoul(cursor, &num_end, 10);
                if (num_end == cursor || n > OBJ_MAX_INDEX)
                    return false;
                cursor = num_end;
            }
        }

        face->v[i] = (u32)v;
        face->t[i] = (u32)t;
        face->n[i] = (u32)n;
    }

    while (*cursor == ' ')
        cursor++;

    /* a fourth vertex ref means a non-triangulated face */
    return *cursor == '\0';
}

static bool load_obj_mesh(string path, mesh_t *mesh_out)
{
    u64 start_ns = OS_TimeNowNs();
    bool result = false;

    scratch_t scratch = Scratch_Begin(g_engine_arena);

    char full_path[MAX_RESOURCE_PATH];
    snprintf(full_path, sizeof(full_path), "%s%.*s", OS_GetBasePath(), (int)path.len, path.str);

    u64 file_size = 0;
    const char *data = (const char *)File_Read(scratch.arena, full_path, &file_size);
    if (!data)
        goto exit;

    const char *file_end = data + file_size;
    char line[OBJ_MAX_LINE];

    // Count elements to size the arrays
    u32 position_count = 0;
    u32 uv_count = 0;
    u32 normal_count = 0;
    u32 face_count = 0;

    for (const char *cursor = data; cursor < file_end;)
    {
        obj_next_line(&cursor, file_end, line, sizeof(line));

        if (line[0] == 'v' && line[1] == ' ')
            position_count++;
        else if (line[0] == 'v' && line[1] == 't' && line[2] == ' ')
            uv_count++;
        else if (line[0] == 'v' && line[1] == 'n' && line[2] == ' ')
            normal_count++;
        else if (line[0] == 'f' && line[1] == ' ')
            face_count++;
    }

    if (position_count == 0 || face_count == 0)
    {
        Log(ERROR, "obj '%s' has no geometry", full_path);
        goto exit;
    }

    if (position_count > OBJ_MAX_INDEX || uv_count > OBJ_MAX_INDEX || normal_count > OBJ_MAX_INDEX)
    {
        Log(ERROR, "obj '%s' exceeds max element count", full_path);
        goto exit;
    }

    vec3 *positions = arena_push_array_no_zero(scratch.arena, vec3, position_count);
    vec2 *uvs = arena_push_array_no_zero(scratch.arena, vec2, uv_count);
    vec3 *normals = arena_push_array_no_zero(scratch.arena, vec3, normal_count);
    obj_face_t *faces = arena_push_array_no_zero(scratch.arena, obj_face_t, face_count);

    // Parse elements
    position_count = 0;
    uv_count = 0;
    normal_count = 0;
    face_count = 0;
    bool found_object = false;

    for (const char *cursor = data; cursor < file_end;)
    {
        obj_next_line(&cursor, file_end, line, sizeof(line));
        bool ok = true;

        if (line[0] == 'v' && line[1] == ' ')
            ok = obj_parse_floats(line + 2, positions[position_count++].Elements, 3);
        else if (line[0] == 'v' && line[1] == 't' && line[2] == ' ')
            ok = obj_parse_floats(line + 3, uvs[uv_count++].Elements, 2);
        else if (line[0] == 'v' && line[1] == 'n' && line[2] == ' ')
            ok = obj_parse_floats(line + 3, normals[normal_count++].Elements, 3);
        else if (line[0] == 'f' && line[1] == ' ')
            ok = obj_parse_face(line + 2, &faces[face_count++]);
        else if (line[0] == 'o' && line[1] == ' ')
        {
            if (found_object)
            {
                Log(ERROR, "obj '%s': multiple objects not supported", full_path);
                goto exit;
            }
            found_object = true;
        }

        if (!ok)
        {
            Log(ERROR, "obj '%s': failed to parse line '%s'", full_path, line);
            goto exit;
        }
    }

    // Deduplicate v/t/n triplets into a single vertex + index stream
    u32 max_vertices = face_count * 3;
    textured_normal_vertex_t *vertices =
        arena_push_array_no_zero(scratch.arena, textured_normal_vertex_t, max_vertices);
    u32 *indices = arena_push_array_no_zero(scratch.arena, u32, max_vertices);
    u32 vertex_count = 0;
    u32 index_count = 0;

    u64 bucket_count = 64;
    while (bucket_count < max_vertices)
        bucket_count <<= 1;
    hash_map_t vertex_map = HashMap_Create(scratch.arena, bucket_count);

    for (u32 face_index = 0; face_index < face_count; face_index++)
    {
        const obj_face_t *face = &faces[face_index];

        /* reversed ref order flips the obj winding to match the engine */
        for (i32 i = 2; i >= 0; i--)
        {
            u32 v = face->v[i];
            u32 t = face->t[i];
            u32 n = face->n[i];

            if (v > position_count || t > uv_count || n > normal_count)
            {
                Log(ERROR, "obj '%s': face index out of range", full_path);
                goto exit;
            }

            u64 key = (u64)v | ((u64)t << 21) | ((u64)n << 42);
            u32 index;
            if (!HashMap_U64U32_Get(&vertex_map, key, &index))
            {
                index = vertex_count++;
                textured_normal_vertex_t *vertex = &vertices[index];
                vertex->position = positions[v - 1];
                vertex->normal = n ? normals[n - 1] : V3(0.0f, 0.0f, 0.0f);
                /* obj uv origin is bottom-left, engine textures are top-left */
                vertex->texture_coord = t ? V2(uvs[t - 1].X, 1.0f - uvs[t - 1].Y)
                                          : V2(0.0f, 0.0f);
                HashMap_U64U32_Insert(&vertex_map, key, index);
            }
            indices[index_count++] = index;
        }
    }

    /* objs without uvs use normal_vertex_t so they render with the same
       pipelines as the predefined normaled meshes */
    // TODO REMOVE
    const void *vertex_data = vertices;
    u64 vertex_data_size = vertex_count * sizeof(textured_normal_vertex_t);
    if (uv_count == 0)
    {
        normal_vertex_t *packed =
            arena_push_array_no_zero(scratch.arena, normal_vertex_t, vertex_count);
        for (u32 i = 0; i < vertex_count; i++)
        {
            packed[i].position = vertices[i].position;
            packed[i].normal = vertices[i].normal;
        }
        vertex_data = packed;
        vertex_data_size = vertex_count * sizeof(normal_vertex_t);
    }

    mesh_out->vertex_buffer = Renderer_CreateStaticVertexBuffer(vertex_data, vertex_data_size);
    mesh_out->index_buffer = Renderer_CreateStaticIndexBuffer(indices, index_count);
    mesh_out->index_count = index_count;
    result = true;

    Log(INFO, "loaded obj '%s' in %.2f ms: %u vertices, %u indices",
        path.c_str, (f64)(OS_TimeNowNs() - start_ns) / 1000000.0, vertex_count, index_count);

exit:
    Scratch_End(scratch);
    return result;
}

static mesh_t *create_mesh(VkBuffer vertex_buffer, VkBuffer index_buffer, u32 index_count)
{
    mesh_t *mesh = arena_push(g_engine_arena, mesh_t);
    mesh->vertex_buffer = vertex_buffer;
    mesh->index_buffer = index_buffer;
    mesh->index_count = index_count;

    return mesh;
}

static void insert_predefined_mesh(predefined_mesh_t slot, VkBuffer vertex_buffer,
                                   VkBuffer index_buffer, u32 index_count)
{
    s_meshes->predefined[slot] = create_mesh(vertex_buffer, index_buffer, index_count);
}

static void load_predefined_meshes(void)
{
    // Triangles
    {
        static const colored_vertex_t colored_vertices[] = {
            {.position = {{-0.5f,  0.5f, 0.0f}}, .color = {{1.0f, 0.0f, 0.0f}}},
            {.position = {{ 0.5f,  0.5f, 0.0f}}, .color = {{0.0f, 1.0f, 0.0f}}},
            {.position = {{-0.5f, -0.5f, 0.0f}}, .color = {{0.0f, 0.0f, 1.0f}}},
        };
        static const simple_vertex_t simple_vertices[] = {
            {.position = {{-0.5f,  0.5f, 0.0f}}},
            {.position = {{ 0.5f,  0.5f, 0.0f}}},
            {.position = {{-0.5f, -0.5f, 0.0f}}},
        };
        static const u32 indices[] = {0, 1, 2};

        VkBuffer colored_vertex_buffer = Renderer_CreateStaticVertexBuffer(colored_vertices, sizeof(colored_vertices));
        VkBuffer simple_vertex_buffer = Renderer_CreateStaticVertexBuffer(simple_vertices, sizeof(simple_vertices));
        VkBuffer index_buffer = Renderer_CreateStaticIndexBuffer(indices, ArrayCount(indices));

        insert_predefined_mesh(PREDEFINED_MESH_SIMPLE_TRIANGLE, simple_vertex_buffer, index_buffer, ArrayCount(indices));
        insert_predefined_mesh(PREDEFINED_MESH_COLORED_TRIANGLE, colored_vertex_buffer, index_buffer, ArrayCount(indices));
    }

    // Quads
    {
        static const simple_vertex_t simple_vertices[] = {
            {.position = {{-0.5f,  0.5f, 0.0f}}},
            {.position = {{ 0.5f,  0.5f, 0.0f}}},
            {.position = {{-0.5f, -0.5f, 0.0f}}},
            {.position = {{ 0.5f, -0.5f, 0.0f}}},
        };
        static const normal_vertex_t normaled_vertices[] = {
            {.position = {{-0.5f,  0.5f, 0.0f}}, .normal = {{0.0f, 0.0f, 1.0f}}},
            {.position = {{ 0.5f,  0.5f, 0.0f}}, .normal = {{0.0f, 0.0f, 1.0f}}},
            {.position = {{-0.5f, -0.5f, 0.0f}}, .normal = {{0.0f, 0.0f, 1.0f}}},
            {.position = {{ 0.5f, -0.5f, 0.0f}}, .normal = {{0.0f, 0.0f, 1.0f}}},
        };
        static const colored_vertex_t colored_vertices[] = {
            {.position = {{-0.5f,  0.5f, 0.0f}}, .color = {{1.0f, 0.0f, 0.0f}}},
            {.position = {{ 0.5f,  0.5f, 0.0f}}, .color = {{0.0f, 1.0f, 0.0f}}},
            {.position = {{-0.5f, -0.5f, 0.0f}}, .color = {{0.0f, 0.0f, 1.0f}}},
            {.position = {{ 0.5f, -0.5f, 0.0f}}, .color = {{1.0f, 0.0f, 1.0f}}},
        };
        static const textured_vertex_t textured_vertices[] = {
            {.position = {{-0.5f,  0.5f, 0.0f}}, .texture_coord = {{0.0f, 0.0f}}},
            {.position = {{ 0.5f,  0.5f, 0.0f}}, .texture_coord = {{1.0f, 0.0f}}},
            {.position = {{-0.5f, -0.5f, 0.0f}}, .texture_coord = {{0.0f, 1.0f}}},
            {.position = {{ 0.5f, -0.5f, 0.0f}}, .texture_coord = {{1.0f, 1.0f}}},
        };
        static const u32 indices[] = {0, 1, 2, 2, 1, 3};

        VkBuffer simple_vertex_buffer = Renderer_CreateStaticVertexBuffer(simple_vertices, sizeof(simple_vertices));
        VkBuffer normaled_vertex_buffer = Renderer_CreateStaticVertexBuffer(normaled_vertices, sizeof(normaled_vertices));
        VkBuffer colored_vertex_buffer = Renderer_CreateStaticVertexBuffer(colored_vertices, sizeof(colored_vertices));
        VkBuffer textured_vertex_buffer = Renderer_CreateStaticVertexBuffer(textured_vertices, sizeof(textured_vertices));
        VkBuffer index_buffer = Renderer_CreateStaticIndexBuffer(indices, ArrayCount(indices));

        insert_predefined_mesh(PREDEFINED_MESH_SIMPLE_QUAD, simple_vertex_buffer, index_buffer, ArrayCount(indices));
        insert_predefined_mesh(PREDEFINED_MESH_NORMALED_QUAD, normaled_vertex_buffer, index_buffer, ArrayCount(indices));
        insert_predefined_mesh(PREDEFINED_MESH_COLORED_QUAD, colored_vertex_buffer, index_buffer, ArrayCount(indices));
        insert_predefined_mesh(PREDEFINED_MESH_TEXTURED_QUAD, textured_vertex_buffer, index_buffer, ArrayCount(indices));
    }

    // Cube
    {
        static const normal_vertex_t cube_vertices[] = {
            // +Z
            {.position = {{-0.5f, -0.5f,  0.5f}}, .normal = {{ 0.0f,  0.0f,  1.0f}}},
            {.position = {{ 0.5f, -0.5f,  0.5f}}, .normal = {{ 0.0f,  0.0f,  1.0f}}},
            {.position = {{ 0.5f,  0.5f,  0.5f}}, .normal = {{ 0.0f,  0.0f,  1.0f}}},
            {.position = {{-0.5f,  0.5f,  0.5f}}, .normal = {{ 0.0f,  0.0f,  1.0f}}},
            // -Z
            {.position = {{ 0.5f, -0.5f, -0.5f}}, .normal = {{ 0.0f,  0.0f, -1.0f}}},
            {.position = {{-0.5f, -0.5f, -0.5f}}, .normal = {{ 0.0f,  0.0f, -1.0f}}},
            {.position = {{-0.5f,  0.5f, -0.5f}}, .normal = {{ 0.0f,  0.0f, -1.0f}}},
            {.position = {{ 0.5f,  0.5f, -0.5f}}, .normal = {{ 0.0f,  0.0f, -1.0f}}},
            // +X
            {.position = {{ 0.5f, -0.5f,  0.5f}}, .normal = {{ 1.0f,  0.0f,  0.0f}}},
            {.position = {{ 0.5f, -0.5f, -0.5f}}, .normal = {{ 1.0f,  0.0f,  0.0f}}},
            {.position = {{ 0.5f,  0.5f, -0.5f}}, .normal = {{ 1.0f,  0.0f,  0.0f}}},
            {.position = {{ 0.5f,  0.5f,  0.5f}}, .normal = {{ 1.0f,  0.0f,  0.0f}}},
            // -X
            {.position = {{-0.5f, -0.5f, -0.5f}}, .normal = {{-1.0f,  0.0f,  0.0f}}},
            {.position = {{-0.5f, -0.5f,  0.5f}}, .normal = {{-1.0f,  0.0f,  0.0f}}},
            {.position = {{-0.5f,  0.5f,  0.5f}}, .normal = {{-1.0f,  0.0f,  0.0f}}},
            {.position = {{-0.5f,  0.5f, -0.5f}}, .normal = {{-1.0f,  0.0f,  0.0f}}},
            // +Y
            {.position = {{-0.5f,  0.5f,  0.5f}}, .normal = {{ 0.0f,  1.0f,  0.0f}}},
            {.position = {{ 0.5f,  0.5f,  0.5f}}, .normal = {{ 0.0f,  1.0f,  0.0f}}},
            {.position = {{ 0.5f,  0.5f, -0.5f}}, .normal = {{ 0.0f,  1.0f,  0.0f}}},
            {.position = {{-0.5f,  0.5f, -0.5f}}, .normal = {{ 0.0f,  1.0f,  0.0f}}},
            // -Y
            {.position = {{-0.5f, -0.5f, -0.5f}}, .normal = {{ 0.0f, -1.0f,  0.0f}}},
            {.position = {{ 0.5f, -0.5f, -0.5f}}, .normal = {{ 0.0f, -1.0f,  0.0f}}},
            {.position = {{ 0.5f, -0.5f,  0.5f}}, .normal = {{ 0.0f, -1.0f,  0.0f}}},
            {.position = {{-0.5f, -0.5f,  0.5f}}, .normal = {{ 0.0f, -1.0f,  0.0f}}},
        };

        static const u32 cube_indices[] = {
            0,  2,  1,  0,  3,  2,  // +Z (clockwise)
            4,  6,  5,  4,  7,  6,  // -Z
            8,  10, 9,  8,  11, 10, // +X
            12, 14, 13, 12, 15, 14, // -X
            16, 18, 17, 16, 19, 18, // +Y
            20, 22, 21, 20, 23, 22, // -Y
        };

        VkBuffer cube_vertex_buffer = Renderer_CreateStaticVertexBuffer(cube_vertices, sizeof(cube_vertices));
        VkBuffer cube_index_buffer = Renderer_CreateStaticIndexBuffer(cube_indices, ArrayCount(cube_indices));

        insert_predefined_mesh(PREDEFINED_MESH_NORMALED_CUBE, cube_vertex_buffer, cube_index_buffer, ArrayCount(cube_indices));
    }
}
