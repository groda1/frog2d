#ifndef PLATFORM_H
#define PLATFORM_H

#include "core.h"

typedef struct platform_window_struct platform_window_t;

// TODO extend as needed (function keys, keypad, mouse)
typedef enum
{
    KEY_UNKNOWN = 0,

    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I,
    KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R,
    KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,

    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,

    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KET_F5, KEY_F6, KEY_F7, KEY_F8,
    KEY_F9, KEY_F10, KEY_F11, KEY_F12,

    KEY_ESCAPE,
    KEY_SPACE,
    KEY_RETURN,
    KEY_TAB,
    KEY_BACKSPACE,

    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,

    KEY_LSHIFT,
    KEY_LCTRL,
    KEY_LALT,

    KEY_COUNT,
} key_code_t;

typedef enum
{
    KEY_EVENT_PASSTHROUGH,
    KEY_EVENT_CONSUMED,
} key_handle_result_t;

typedef enum
{
    PLATFORM_EVENT_QUIT,
    PLATFORM_EVENT_KEY_DOWN,
    PLATFORM_EVENT_KEY_UP,
    PLATFORM_EVENT_WINDOW_RESIZED,
} platform_event_type_t;

typedef struct
{
    platform_event_type_t type;
    union
    {
        key_code_t key;
        struct
        {
            u32 width;
            u32 height;
        } resize;
    };
} platform_event_t;

bool Platform_Init(void);
void Platform_Shutdown(void);

platform_window_t *Platform_CreateWindow(const char *title, u32 width, u32 height);
void Platform_DestroyWindow(platform_window_t *window);
void Platform_ShowWindow(platform_window_t *window);
bool Platform_GetWindowSize(platform_window_t *window, u32 *width, u32 *height);

bool Platform_PollEvent(platform_event_t *event);

#endif
