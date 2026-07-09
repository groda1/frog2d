#ifndef GAME_MAIN_H
#define GAME_MAIN_H

#include <SDL3/SDL_keycode.h>

#include "core.h"

void Game_HandleKeyDown(SDL_Keycode key);
void Game_HandleKeyUp(SDL_Keycode key);
void Game_Tick(f32 delta_time);

#endif
