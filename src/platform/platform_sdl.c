#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_error.h>

#include "log.h"

#include "platform.h"
#include "platform_vulkan.h"

struct platform_window_struct
{
    SDL_Window *sdl_window;
};

// TODO make dynamic if multiple windows are ever needed
static platform_window_t s_window;

const char *__lsan_default_suppressions(void);

bool Platform_Init(void)
{
    SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_SCALE_TO_DISPLAY, "1");

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        Log(ERROR, "Failed to init SDL: %s", SDL_GetError());
        return false;
    }

    return true;
}

void Platform_Shutdown(void)
{
    SDL_Quit();
}

platform_window_t *Platform_CreateWindow(const char *title, u32 width, u32 height)
{
    SDL_Window *window = SDL_CreateWindow(
        title, (int)width, (int)height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        Log(ERROR, "Failed to create window: %s", SDL_GetError());
        return NULL;
    }

    SDL_SetWindowMinimumSize(window, 640, 480);
    SDL_SetWindowMaximumSize(window, 3840, 2160);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    s_window.sdl_window = window;
    return &s_window;
}

void Platform_DestroyWindow(platform_window_t *window)
{
    SDL_DestroyWindow(window->sdl_window);
    window->sdl_window = NULL;
}

void Platform_ShowWindow(platform_window_t *window)
{
    SDL_ShowWindow(window->sdl_window);
}

bool Platform_GetWindowSize(platform_window_t *window, u32 *width, u32 *height)
{
    int w, h;
    if (!SDL_GetWindowSize(window->sdl_window, &w, &h))
        return false;

    *width = (u32)w;
    *height = (u32)h;
    return true;
}

static key_code_t translate_keycode(SDL_Keycode key)
{
    if (key >= SDLK_A && key <= SDLK_Z)
        return (key_code_t)(KEY_A + (key - SDLK_A));
    if (key >= SDLK_0 && key <= SDLK_9)
        return (key_code_t)(KEY_0 + (key - SDLK_0));
    if (key >= SDLK_F1 && key <= SDLK_F12)
        return (key_code_t)(KEY_F1 + (key - SDLK_F1));

    switch (key)
    {
        case SDLK_ESCAPE:    return KEY_ESCAPE;
        case SDLK_SPACE:     return KEY_SPACE;
        case SDLK_RETURN:    return KEY_RETURN;
        case SDLK_TAB:       return KEY_TAB;
        case SDLK_BACKSPACE: return KEY_BACKSPACE;
        case SDLK_LEFT:      return KEY_LEFT;
        case SDLK_RIGHT:     return KEY_RIGHT;
        case SDLK_UP:        return KEY_UP;
        case SDLK_DOWN:      return KEY_DOWN;
        case SDLK_LSHIFT:    return KEY_LSHIFT;
        case SDLK_LCTRL:     return KEY_LCTRL;
        case SDLK_LALT:      return KEY_LALT;
        case SDLK_PAGEUP:    return KEY_PGUP;
        case SDLK_PAGEDOWN:  return KEY_PGDN;
        default:             return KEY_UNKNOWN;
    }
}

bool Platform_PollEvent(platform_event_t *event)
{
    SDL_Event sdl_event;
    while (SDL_PollEvent(&sdl_event))
    {
        switch (sdl_event.type)
        {
        case SDL_EVENT_QUIT:
            event->type = PLATFORM_EVENT_QUIT;
            return true;

        case SDL_EVENT_KEY_DOWN:
            event->type = PLATFORM_EVENT_KEY_DOWN;
            event->key = translate_keycode(sdl_event.key.key);
            return true;

        case SDL_EVENT_KEY_UP:
            event->type = PLATFORM_EVENT_KEY_UP;
            event->key = translate_keycode(sdl_event.key.key);
            return true;

        case SDL_EVENT_WINDOW_RESIZED:
            event->type = PLATFORM_EVENT_WINDOW_RESIZED;
            event->resize.width = (u32)sdl_event.window.data1;
            event->resize.height = (u32)sdl_event.window.data2;
            return true;

        default:
            break;
        }
    }

    return false;
}

const char *const *Platform_Vulkan_GetInstanceExtensions(u32 *count)
{
    return SDL_Vulkan_GetInstanceExtensions(count);
}

bool Platform_Vulkan_CreateSurface(platform_window_t *window, VkInstance instance,
                                   VkSurfaceKHR *surface)
{
    if (!SDL_Vulkan_CreateSurface(window->sdl_window, instance, NULL, surface))
    {
        Log(ERROR, "Failed to create surface: %s", SDL_GetError());
        return false;
    }

    return true;
}

const char *__lsan_default_suppressions(void)
{
    return "leak:libdbus-1\n";
}
