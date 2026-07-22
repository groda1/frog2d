#include "core.h"
#include "core_math.h"
#include "core_string.h"

/*
 *
 * FILE
 *   magic              u8[8]      "FROGMODL"
 *   version            u16
 *   triangle_count     u32        T  — 3·T unshared vertices; shared by every keyframe
 *   material_count     u16        P
 *   anchor_count       u16        A
 *   animation_count    u16        M
 *
 *   materials  × material_count (P):
 *       name_len       u16
 *       name           u8[name_len]        slot name: "skin", "hair", ...
 *       base_color     f32[3]     12
 *       specular_color f32[3]     12
 *
 *   triangle materials  × triangle_count (T):           [1 byte each]
 *       material       u8         1        index into the palette above
 *
 *   anchor names  × anchor_count (A):
 *       name_len       u16
 *       name           u8[name_len]
 *
 *   animations  × animation_count (M):
 *       name_len       u16
 *       name           u8[name_len]
 *       keyframe_count u16        K
 *
 *       keyframes  × keyframe_count (K):
 *           time       f32        seconds; 0.0 for the first keyframe
 *
 *           positions  × 3·T vertices, CW winding:           [12 bytes each]
 *               position   f32[3]     12
 *
 *           anchor transforms  × anchor_count (A):           [28 bytes each]
 *               position     f32[3]   12
 *               orientation  f32[4]   16   quat, xyzw
 */

typedef struct {
    string name;
    vec3 base_color;
    vec3 specular_color;
} model_material_t;



typedef struct {
    VkBuffer vertex_buffer;
    VkBuffer index_buffer;
    u32 index_count;

    model_anchor_t *anchors;

} model_keyframe_t;

typedef struct {
    string name;
    u16 keyframe_count;


} model_animation_t;

typedef struct {

    u16 anchor_count;
    u16 animation_count;




} model_t;
