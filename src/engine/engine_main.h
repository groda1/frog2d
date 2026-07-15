#ifndef ENGINE_MAIN_H
#define ENGINE_MAIN_H

#include "core.h"
#include "platform.h"

bool Engine_Init(platform_window_t *window);
void Engine_Destroy(void);

void Engine_HandleResize(u32 width, u32 height);

/* returns the seconds elapsed since the previous frame */
f32  Engine_BeginFrame(void);
void Engine_EndFrame(void);

#endif
