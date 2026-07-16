#ifndef CONSOLE_H
#define CONSOLE_H

#include "core.h"
#include "platform.h"

bool Console_Init();
void Console_Destroy();

void Console_Update(u32 delta_time);
void Console_Draw();

key_handle_result_t Console_HandleKeyDown(key_code_t key);
key_handle_result_t Console_HandleKeyUp(key_code_t key);

#endif
