#ifndef MATH_H
#define MATH_H

#include "core.h"
#include "HandmadeMath.h"


#define V2 HMM_V2
#define V3 HMM_V3
#define V4 HMM_V4

typedef HMM_Vec2 vec2;
typedef HMM_Vec3 vec3;
typedef HMM_Vec4 vec4;

typedef HMM_Mat4 mat4;

typedef HMM_Quat quat;

#define lerp HMM_Lerp

inline f32 smoothstep(f32 t) { return t * t * (3.0f - 2.0f * t); }
inline f32 smootherstep(f32 t) { return t * t * t * (t * (6 * t - 15) + 10); }

#endif
