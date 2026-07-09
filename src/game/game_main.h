#ifndef GAME_MAIN_H
#define GAME_MAIN_H

#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_video.h>

#include "core.h"

bool Game_Init(SDL_Window *window);
void Game_Destroy(void);

void Game_HandleKeyDown(SDL_Keycode key);
void Game_HandleKeyUp(SDL_Keycode key);
void Game_HandleResize(u32 width, u32 height);
void Game_Tick(void);

#endif
