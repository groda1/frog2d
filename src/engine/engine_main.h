#ifndef ENGINE_MAIN_H
#define ENGINE_MAIN_H

#include "core.h"
#include "platform.h"

bool Engine_Init(platform_window_t *window);
void Engine_Destroy(void);

void Engine_HandleResize(u32 width, u32 height);
key_handle_result_t Engine_HandleKeyDown(key_code_t key);
key_handle_result_t Engine_HandleKeyUp(key_code_t key);

/* returns the seconds elapsed since the previous frame */
f32  Engine_BeginFrame(void);
void Engine_EndFrame(void);

#endif
