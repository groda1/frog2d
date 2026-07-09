
#ifndef TEXT_H
#define TEXT_H

#include "core.h"
#include "core_string.h"
#include "core_math.h"
#include "memory_arena.h"

bool Text_Init(arena_t *arena, u64 initial_character_cap);
void Text_Destroy();
void Text_HandleResize(u32 width, u32 height);

void Text_SetSize(u32 size);
void Text_SetColor(vec4 color);

bool Text_Draw(u32 x, u32 y, string text);

void Text_BeginFrame();
void Text_EndFrame();

#endif
