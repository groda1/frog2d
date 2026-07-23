#ifndef MODEL_H
#define MODEL_H

#include "core.h"
#include "core_math.h"
#include "core_string.h"

#include "mesh.h"

#define MODEL_INVALID_HANDLE NULL

typedef struct _model_t             model_t;
typedef struct _model_material_t    model_material_t;
typedef struct _model_keyframe_t    model_keyframe_t;
typedef struct _model_anchor_t      model_anchor_t;
typedef struct _model_animation_t   model_animation_t;

typedef const model_t *model_handle_t;

#

struct _model_material_t
{
    string name;
    vec3 base_color;
    vec3 specular_color;
};

struct _model_anchor_t
{
    vec3 pos;
    quat orientation;
};

struct _model_keyframe_t {
    f32 time_s;
    mesh_handle_t mesh;
    model_anchor_t *anchors;
};

struct _model_animation_t
{
    string name;
    u16 keyframe_count;

    model_keyframe_t *keyframes;
};

struct _model_t
{
    u16 material_count;
    u16 anchor_count;
    u16 animation_count;

    model_material_t *materials;
    string *anchor_names;
    model_animation_t *animations;
};


#endif
