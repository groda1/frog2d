#ifndef ENGINE_MAIN_H
#define ENGINE_MAIN_H

#include <SDL3/SDL_video.h>

#include "core.h"

bool Engine_Init(SDL_Window *window);
void Engine_Destroy(void);

void Engine_HandleResize(u32 width, u32 height);

/* returns the seconds elapsed since the previous frame */
f32  Engine_BeginFrame(void);
void Engine_EndFrame(void);

#endif
