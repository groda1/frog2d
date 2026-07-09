#include "core.h"
#include "hash_map.h"
#include "log.h"
#include "memory_arena.h"

#include "mesh.h"
#include "vulkan_renderer.h"

#define MESH_START_HANDLE 1000
#define MESH_MAP_BUCKET_COUNT 64

typedef struct
{
    arena_t *arena;
    hash_map_t meshes;
    mesh_handle_t next_handle;
} mesh_manager_t;

static mesh_manager_t *g_mesh_manager;

static VkBuffer create_static_vertex_buffer(const void *vertices, u64 size);
static VkBuffer create_static_index_buffer(const u32 *indices, u32 index_count);
static bool load_obj_mesh(string path, mesh_t *mesh_out);
static mesh_t *insert_mesh(mesh_handle_t handle, VkBuffer vertex_buffer, VkBuffer index_buffer, u32 index_count);
static void load_predefined_meshes(void);

bool MeshManager_Init(arena_t *arena)
{
    g_mesh_manager = arena_push(arena, mesh_manager_t);
    g_mesh_manager->arena = arena;
    g_mesh_manager->meshes = HashMap_Create(arena, MESH_MAP_BUCKET_COUNT);
    g_mesh_manager->next_handle = MESH_START_HANDLE;

    load_predefined_meshes();

    return true;
}

mesh_t *MeshManager_GetMesh(mesh_handle_t handle)
{
    mesh_t *mesh = HashMap_U32Ptr_Get(&g_mesh_manager->meshes, handle);
    AssertAlways(mesh != NULL);

    return mesh;
}

mesh_t *MeshManager_LoadMesh(string path, mesh_handle_t *handle_out)
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
        Log(ERROR, "unknown file type: %.*s", (int)path.len, path.str);
        return NULL;
    }

    if (string_match(extension, string_lit("obj")))
    {
        mesh_t mesh;
        if (!load_obj_mesh(path, &mesh))
            return NULL;

        mesh_handle_t handle = g_mesh_manager->next_handle++;
        if (handle_out)
            *handle_out = handle;

        return insert_mesh(handle, mesh.vertex_buffer, mesh.index_buffer, mesh.index_count);
    }

    Log(ERROR, "failed to load mesh: %.*s", (int)path.len, path.str);
    return NULL;
}

static VkBuffer create_static_vertex_buffer(const void *vertices, u64 size)
{
    VkBuffer buffer = VulkanRenderer_CreateStaticVertexBuffer(vertices, size);
    AssertAlways(buffer != VK_NULL_HANDLE);

    return buffer;
}

static VkBuffer create_static_index_buffer(const u32 *indices, u32 index_count)
{
    VkBuffer buffer = VulkanRenderer_CreateStaticIndexBuffer(indices, index_count);
    AssertAlways(buffer != VK_NULL_HANDLE);

    return buffer;
}

static bool load_obj_mesh(string path, mesh_t *mesh_out)
{
    // TODO port obj loading
    (void)mesh_out;

    Log(WARNING, "obj loading not implemented: %.*s", (int)path.len, path.str);
    return false;
}

static mesh_t *insert_mesh(mesh_handle_t handle, VkBuffer vertex_buffer, VkBuffer index_buffer, u32 index_count)
{
    mesh_t *mesh = arena_push(g_mesh_manager->arena, mesh_t);
    mesh->vertex_buffer = vertex_buffer;
    mesh->index_buffer = index_buffer;
    mesh->index_count = index_count;

    HashMap_U32Ptr_Insert(&g_mesh_manager->meshes, handle, mesh);

    return mesh;
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

        VkBuffer colored_vertex_buffer = create_static_vertex_buffer(colored_vertices, sizeof(colored_vertices));
        VkBuffer simple_vertex_buffer = create_static_vertex_buffer(simple_vertices, sizeof(simple_vertices));
        VkBuffer index_buffer = create_static_index_buffer(indices, ArrayCount(indices));

        insert_mesh(PREDEFINED_MESH_SIMPLE_TRIANGLE, simple_vertex_buffer, index_buffer, ArrayCount(indices));
        insert_mesh(PREDEFINED_MESH_COLORED_TRIANGLE, colored_vertex_buffer, index_buffer, ArrayCount(indices));
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

        VkBuffer simple_vertex_buffer = create_static_vertex_buffer(simple_vertices, sizeof(simple_vertices));
        VkBuffer normaled_vertex_buffer = create_static_vertex_buffer(normaled_vertices, sizeof(normaled_vertices));
        VkBuffer colored_vertex_buffer = create_static_vertex_buffer(colored_vertices, sizeof(colored_vertices));
        VkBuffer textured_vertex_buffer = create_static_vertex_buffer(textured_vertices, sizeof(textured_vertices));
        VkBuffer index_buffer = create_static_index_buffer(indices, ArrayCount(indices));

        insert_mesh(PREDEFINED_MESH_SIMPLE_QUAD, simple_vertex_buffer, index_buffer, ArrayCount(indices));
        insert_mesh(PREDEFINED_MESH_NORMALED_QUAD, normaled_vertex_buffer, index_buffer, ArrayCount(indices));
        insert_mesh(PREDEFINED_MESH_COLORED_QUAD, colored_vertex_buffer, index_buffer, ArrayCount(indices));
        insert_mesh(PREDEFINED_MESH_TEXTURED_QUAD, textured_vertex_buffer, index_buffer, ArrayCount(indices));
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

        VkBuffer cube_vertex_buffer = create_static_vertex_buffer(cube_vertices, sizeof(cube_vertices));
        VkBuffer cube_index_buffer = create_static_index_buffer(cube_indices, ArrayCount(cube_indices));

        insert_mesh(PREDEFINED_MESH_NORMALED_CUBE, cube_vertex_buffer, cube_index_buffer, ArrayCount(cube_indices));
    }
}
