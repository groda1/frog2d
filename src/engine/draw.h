#ifndef DRAW_H
#define DRAW_H

#include "core.h"
#include "core_string.h"
#include "core_math.h"
#include "render_types.h"

bool Draw_Init();
void Draw_Destroy();
void Draw_HandleResize(u32 width, u32 height);

bool Draw_Quad(u32 x, u32 y, u32 width, u32 height, vec4 color);
bool Draw_TexturedQuad(u32 x, u32 y, u32 width, u32 height, vec4 color, texture_handle_t texture);

void Draw_SetTextSize(u32 size);
void Draw_SetTextColor(vec4 color);
bool Draw_Text(u32 x, u32 y, string text);

void Draw_BeginFrame();
void Draw_EndFrame();

#endif
