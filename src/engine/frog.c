
#include <stdio.h>
#include <vulkan/vulkan_core.h>

#include "HandmadeMath.h"
#include "core.h"
#include "core_string.h"
#include "log.h"
#include "memory_arena.h"

#include "mesh.h"
#include "mesh_internal.h"
#include "model.h"
#include "renderer.h"

#define MAGIC 0x4C444F4D474F5246 // "FROGMODL" little endian
extern arena_t *g_engine_arena;
extern arena_t *g_scratch;

static string read_string(arena_t *arena, FILE *file);
static bool read_vec3(vec3 *out, FILE *file);
static bool read_quat(quat *out, FILE *file);
/*
    FILE
      magic              u8[8]      "FROGMODL"
      version            u16
      triangle_count     u32        T  — 3·T unshared vertices; shared by every keyframe
      material_count     u16        P
      anchor_count       u16        A
      animation_count    u16        M

      materials  × material_count (P):
          name_len       u16
          name           u8[name_len]        slot name: "skin", "hair", ...
          base_color     f32[3]     12
          specular_color f32[3]     12

      triangle materials  × triangle_count (T):           [1 byte each]
          material       u8         1        index into the palette above

      anchor names  × anchor_count (A):
          name_len       u16
          name           u8[name_len]

      animations  × animation_count (M):
          name_len       u16
          name           u8[name_len]
          keyframe_count u16        K

          keyframes  × keyframe_count (K):
              time       f32        seconds; 0.0 for the first keyframe

              positions  × 3·T vertices, CW winding:           [12 bytes each]
                  position   f32[3]     12

              anchor transforms  × anchor_count (A):           [28 bytes each]
                  position     f32[3]   12
                  orientation  f32[4]   16   quat, xyzw

*/


typedef struct
{
    u64 magic;
    u16 version;
    u32 triangle_count;
    u16 material_count;
    u16 anchor_count;
    u16 animation_count;

} AttributePacked frog_header_t;

model_handle_t Frog_LoadModel(const char *path)
{
    frog_header_t header;
    model_t *model = NULL;
    model_handle_t handle = MODEL_INVALID_HANDLE;
    u64 pos = MemoryArena_Pos(g_engine_arena);

    FILE *file = fopen(path, "rb");
    if (!file)
    {
        Log(ERROR, "failed to open file: %s", path);
        return MODEL_INVALID_HANDLE;
    }

    if (!fread(&header, sizeof(header), 1, file))
    {
        Log(ERROR, "failed to read frog header from file: %s", path);
        goto exit;
    }

    Log(DEBUG, "read frog header from file: %s { magic=%lX version=%u tri=%u mat=%u anchors=%u anims=%u",
        path, header.magic, header.version, header.triangle_count, header.material_count, header.anchor_count, header.animation_count);

    Assert(g_engine_arena != NULL);
    Assert(g_scratch != NULL);

    model = arena_push(g_engine_arena, model_t);

    model->material_count = header.material_count;
    model->materials = arena_push_array(g_engine_arena, model_material_t, model->material_count);
    model->anchor_count = header.anchor_count;
    model->anchor_names = arena_push_array(g_engine_arena, string, model->anchor_count);
    model->animation_count = header.animation_count;
    model->animations = arena_push_array(g_engine_arena, model_animation_t, model->animation_count);

    // Materials
    for (u32 i = 0; i < model->material_count; i++)
    {
        model_material_t *material = &model->materials[i];

        material->name = read_string(g_engine_arena, file);
        if (!material->name.len)
            goto fail;

        if (!read_vec3(&material->base_color, file))
            goto fail;
        if (!read_vec3(&material->specular_color, file))
            goto fail;

        Log(DEBUG, "read material name=%S base=%v3 spec=%v3", material->name, material->base_color,
            material->specular_color);
    }

    // Triangle material index
    u8 *triangle_materials = arena_push_array(g_scratch, u8, header.triangle_count);
    if (!fread(triangle_materials, header.triangle_count, 1, file))
        goto fail;

    // Anchor names
    for (u32 i = 0; i < model->anchor_count; i++)
    {
        string *ptr = &model->anchor_names[i];
        *ptr = read_string(g_engine_arena, file);
        if (!ptr->len)
            goto fail;
        Log(DEBUG, "read anchor name: %S", *ptr);
    }

    u32 index_count = header.triangle_count * 3;
    u32 *indices = arena_push_array(g_scratch, u32, index_count);
    for (u32 i = 0; i < index_count; i++)
        indices[i] = i;
    VkBuffer index_buffer = Renderer_CreateStaticIndexBuffer(indices, index_count);

    // Animations
    for (u32 anim_idx = 0; anim_idx < model->animation_count; anim_idx++)
    {
        Log(DEBUG, "anim %u", anim_idx);
        model_animation_t *animation = &model->animations[anim_idx];

        animation->name = read_string(g_engine_arena, file);
        if (!animation->name.len)
            goto fail;

        if (!fread(&animation->keyframe_count, sizeof(u16), 1, file))
            goto fail;

        Log(DEBUG, "read animation header (%S, keyframes=%u)", animation->name, animation->keyframe_count);

        animation->keyframes = arena_push_array(g_engine_arena, model_keyframe_t, animation->keyframe_count);

        for (u32 key_idx = 0; key_idx < animation->keyframe_count; key_idx++)
        {
            model_keyframe_t *keyframe = &animation->keyframes[key_idx];

            if (!fread(&keyframe->time_s, sizeof(f32), 1, file))
                goto fail;
            Log(DEBUG, "read anim=%u keyframe %u time=%f", anim_idx, key_idx, keyframe->time_s);

            normal_material_vertex_t *vertex_data = arena_push_array(g_scratch, normal_material_vertex_t, header.triangle_count * 3);
            for (u32 tri_idx =0; tri_idx < header.triangle_count; tri_idx++)
            {
                normal_material_vertex_t *v0 = &vertex_data[tri_idx * 3];
                normal_material_vertex_t *v1 = &vertex_data[tri_idx * 3 + 1];
                normal_material_vertex_t *v2 = &vertex_data[tri_idx * 3 + 2];

                if (!read_vec3(&v0->position, file))
                    goto fail;
                if (!read_vec3(&v1->position, file))
                    goto fail;
                if (!read_vec3(&v2->position, file))
                    goto fail;

                vec3 normal =
                    HMM_NormV3(HMM_Cross(HMM_SubV3(v2->position, v0->position), HMM_SubV3(v1->position, v0->position)));

                v0->normal = normal;
                v1->normal = normal;
                v2->normal = normal;

                v0->material = triangle_materials[tri_idx];
                v1->material = triangle_materials[tri_idx];
                v2->material = triangle_materials[tri_idx];
                Log(DEBUG, "read triangle keyframe=%u: %v3 %v3 %v3 normal=%v3", key_idx, v0->position, v1->position, v2->position, normal);
            }

            VkBuffer vertex_buffer = Renderer_CreateStaticVertexBuffer(vertex_data, sizeof(normal_material_vertex_t) * header.triangle_count * 3);

            mesh_t *mesh = arena_push(g_engine_arena, mesh_t);
            mesh->vertex_buffer = vertex_buffer;
            mesh->index_buffer = index_buffer;
            mesh->index_count = index_count;

            Log(DEBUG, "created mesh vertex=%u index=%u count=%u", mesh->vertex_buffer, mesh->index_buffer, mesh->index_count);
            keyframe->mesh = mesh;


            keyframe->anchors = arena_push_array(g_engine_arena, model_anchor_t, header.anchor_count);
            for (u32 anchor_idx = 0; anchor_idx < header.anchor_count; anchor_idx++)
            {
                model_anchor_t *anchor = &keyframe->anchors[anchor_idx];
                if (!read_vec3(&anchor->pos, file))
                    goto fail;
                if (!read_quat(&anchor->orientation, file))
                    goto fail;
                Log(DEBUG, "read anchor keyframe=%u: %v3 %v4", anchor->pos, anchor->orientation);
            }
        }
    }

    handle = model;
exit:
    fclose(file);
    MemoryArena_Clear(g_scratch);
    return handle;

fail:
    Log(ERROR, "failed to frog file: %s", path);
    fclose(file);
    MemoryArena_Clear(g_scratch);
    MemoryArena_PopTo(g_engine_arena, pos);
    return MODEL_INVALID_HANDLE;
}

static string read_string(arena_t *arena, FILE *file)
{
    u16 name_len;

    u64 pos = MemoryArena_Pos(arena);
    Assert(file);
    if (!fread(&name_len, sizeof(u16), 1, file))
        goto fail;

    string s = string_new(arena, name_len);

    if (!fread(s.str, name_len, 1, file))
        goto fail;
    s.len = name_len;
    return s;

fail:
    MemoryArena_PopTo(arena, pos);
    return string_empty();
}

static bool read_vec3(vec3 *out, FILE *file)
{
    if (!fread(out->Elements, sizeof(out->Elements), 1, file))
        return false;
    return true;
}

static bool read_quat(quat *out, FILE *file)
{
    if (!fread(out->Elements, sizeof(out->Elements), 1, file))
        return false;
    return true;
}
