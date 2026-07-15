#ifndef GAME_MAIN_H
#define GAME_MAIN_H

#include "core.h"
#include "platform.h"

bool Game_Init(platform_window_t *window);
void Game_Destroy(void);

void Game_HandleKeyDown(key_code_t key);
void Game_HandleKeyUp(key_code_t key);
void Game_HandleResize(u32 width, u32 height);
void Game_Tick(void);

#endif
