
#include <stdio.h>
#include <vulkan/vulkan_core.h>

#include "core.h"
#include "core_string.h"
#include "log.h"
#include "memory_arena.h"

#include "model.h"

#define MAGIC 0x4C444F4D474F5246 // "FROGMODL" little endian
extern arena_t *g_engine_arena;
extern arena_t *g_scratch;

static string read_string(arena_t *arena, FILE *file);
static bool read_vec3(vec3 *out, FILE *file);
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
    for (int i = 0; i < model->material_count; i++)
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
    for (int i = 0; i < model->anchor_count; i++)
    {
        string *ptr = &model->anchor_names[i];
        *ptr = read_string(g_engine_arena, file);
        if (!ptr->len)
            goto fail;
        Log(DEBUG, "read anchor name: %S", *ptr);
    }

    // Animations
    for (int i = 0; i< model->material_count; i++)
    {
        model_animation_t *animation = &model->animations[i];

        animation->name = read_string(g_engine_arena, file);
        if (!animation->name.len)
            goto fail;

        if (!fread(&animation->keyframe_count, sizeof(u16), 1, file))
            goto fail;

        Log(DEBUG, "read animation header (%S, keyframes=%u)", animation->name, animation->keyframe_count);

        animation->keyframes = arena_push_array(g_engine_arena, model_keyframe_t, animation->keyframe_count);

    }


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
