#ifndef ENGINE_MAIN_H
#define ENGINE_MAIN_H

#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_video.h>

bool Engine_Init(SDL_Window *window);
void Engine_Destroy(void);

void Engine_HandleKeyDown(SDL_Keycode key);
void Engine_HandleKeyUp(SDL_Keycode key);
void Engine_Tick(void);

#endif
