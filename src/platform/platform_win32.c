#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "log.h"

#include "platform.h"
#include "platform_vulkan.h"

#define WINDOW_CLASS_NAME  L"dcfs"
#define WINDOW_STYLE       WS_OVERLAPPEDWINDOW
#define MAX_PENDING_EVENTS 64
#define MAX_WINDOW_TITLE   256

#define WINDOW_MIN_WIDTH   640
#define WINDOW_MIN_HEIGHT  480
#define WINDOW_MAX_WIDTH   3840
#define WINDOW_MAX_HEIGHT  2160

struct platform_window_struct
{
    HWND hwnd;
    u32 width;
    u32 height;
};

// TODO make dynamic if multiple windows are ever needed
static platform_window_t s_window;
static HINSTANCE s_instance;

static platform_event_t s_pending_events[MAX_PENDING_EVENTS];
static u32 s_pending_head;
static u32 s_pending_count;

static void push_event(const platform_event_t *event)
{
    if (s_pending_count == MAX_PENDING_EVENTS)
    {
        Log(WARNING, "event queue full, dropping event");
        return;
    }

    s_pending_events[(s_pending_head + s_pending_count) % MAX_PENDING_EVENTS] = *event;
    s_pending_count++;
}

static bool pop_event(platform_event_t *event)
{
    if (s_pending_count == 0)
        return false;

    *event = s_pending_events[s_pending_head];
    s_pending_head = (s_pending_head + 1) % MAX_PENDING_EVENTS;
    s_pending_count--;
    return true;
}

static key_code_t translate_keycode(WPARAM key)
{
    if (key >= 'A' && key <= 'Z')
        return (key_code_t)(KEY_A + (key - 'A'));
    if (key >= '0' && key <= '9')
        return (key_code_t)(KEY_0 + (key - '0'));
    if (key >= VK_F1 && key <= VK_F12)
        return (key_code_t)(KEY_F1 + (key - VK_F1));
    if (key >= VK_NUMPAD1 && key <= VK_NUMPAD9)
        return (key_code_t)(KEY_KP_1 + (key - VK_NUMPAD1));

    switch (key)
    {
        case VK_ESCAPE:  return KEY_ESCAPE;
        case VK_SPACE:   return KEY_SPACE;
        case VK_RETURN:  return KEY_RETURN;
        case VK_TAB:     return KEY_TAB;
        case VK_BACK:    return KEY_BACKSPACE;
        case VK_LEFT:    return KEY_LEFT;
        case VK_RIGHT:   return KEY_RIGHT;
        case VK_UP:      return KEY_UP;
        case VK_DOWN:    return KEY_DOWN;
        // TODO distinguish left/right modifiers via the lparam extended bit
        case VK_SHIFT:   return KEY_LSHIFT;
        case VK_CONTROL: return KEY_LCTRL;
        case VK_MENU:    return KEY_LALT;
        case VK_PRIOR:   return KEY_PGUP;
        case VK_NEXT:    return KEY_PGDN;
        case VK_HOME:    return KEY_HOME;
        case VK_END:     return KEY_END;
        case VK_NUMPAD0: return KEY_KP_0;
        default:         return KEY_UNKNOWN;
    }
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    platform_event_t event;

    switch (msg)
    {
    case WM_CLOSE:
        event.type = PLATFORM_EVENT_QUIT;
        push_event(&event);
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        event.type = PLATFORM_EVENT_KEY_DOWN;
        event.key = translate_keycode(wparam);
        push_event(&event);
        return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        event.type = PLATFORM_EVENT_KEY_UP;
        event.key = translate_keycode(wparam);
        push_event(&event);
        return 0;

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *info = (MINMAXINFO *)lparam;

        RECT min_rect = {0, 0, WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT};
        RECT max_rect = {0, 0, WINDOW_MAX_WIDTH, WINDOW_MAX_HEIGHT};
        AdjustWindowRect(&min_rect, WINDOW_STYLE, FALSE);
        AdjustWindowRect(&max_rect, WINDOW_STYLE, FALSE);

        info->ptMinTrackSize.x = min_rect.right - min_rect.left;
        info->ptMinTrackSize.y = min_rect.bottom - min_rect.top;
        info->ptMaxTrackSize.x = max_rect.right - max_rect.left;
        info->ptMaxTrackSize.y = max_rect.bottom - max_rect.top;
        return 0;
    }

    case WM_SIZE:
    {
        u32 width = LOWORD(lparam);
        u32 height = HIWORD(lparam);

        /* zero size means minimized */
        if (width == 0 || height == 0)
            return 0;
        if (width == s_window.width && height == s_window.height)
            return 0;

        s_window.width = width;
        s_window.height = height;

        event.type = PLATFORM_EVENT_WINDOW_RESIZED;
        event.resize.width = width;
        event.resize.height = height;
        push_event(&event);
        return 0;
    }

    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

bool Platform_Init(void)
{
    s_instance = GetModuleHandleW(NULL);

    // TODO per-monitor v2 dpi awareness + WM_DPICHANGED handling
    SetProcessDPIAware();

    WNDCLASSEXW window_class = {
        .cbSize = sizeof(WNDCLASSEXW),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = wnd_proc,
        .hInstance = s_instance,
        .hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW),
        .lpszClassName = WINDOW_CLASS_NAME,
    };

    if (!RegisterClassExW(&window_class))
    {
        Log(ERROR, "Failed to register window class: %lu", GetLastError());
        return false;
    }

    return true;
}

void Platform_Shutdown(void)
{
    UnregisterClassW(WINDOW_CLASS_NAME, s_instance);
}

platform_window_t *Platform_CreateWindow(const char *title, u32 width, u32 height)
{
    RECT rect = {0, 0, (LONG)width, (LONG)height};
    AdjustWindowRect(&rect, WINDOW_STYLE, FALSE);

    int window_width = rect.right - rect.left;
    int window_height = rect.bottom - rect.top;
    int x = (GetSystemMetrics(SM_CXSCREEN) - window_width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - window_height) / 2;

    WCHAR title_w[MAX_WINDOW_TITLE];
    MultiByteToWideChar(CP_UTF8, 0, title, -1, title_w, MAX_WINDOW_TITLE);

    /* set before CreateWindowExW: wnd_proc sees WM_SIZE during creation */
    s_window.width = width;
    s_window.height = height;

    HWND hwnd = CreateWindowExW(0, WINDOW_CLASS_NAME, title_w, WINDOW_STYLE,
                                x, y, window_width, window_height,
                                NULL, NULL, s_instance, NULL);
    if (!hwnd)
    {
        Log(ERROR, "Failed to create window: %lu", GetLastError());
        return NULL;
    }

    s_window.hwnd = hwnd;
    return &s_window;
}

void Platform_DestroyWindow(platform_window_t *window)
{
    DestroyWindow(window->hwnd);
    window->hwnd = NULL;
}

void Platform_ShowWindow(platform_window_t *window)
{
    ShowWindow(window->hwnd, SW_SHOW);
}

bool Platform_GetWindowSize(platform_window_t *window, u32 *width, u32 *height)
{
    RECT rect;
    if (!GetClientRect(window->hwnd, &rect))
        return false;

    *width = (u32)(rect.right - rect.left);
    *height = (u32)(rect.bottom - rect.top);
    return true;
}

bool Platform_PollEvent(platform_event_t *event)
{
    if (pop_event(event))
        return true;

    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            platform_event_t quit_event = {.type = PLATFORM_EVENT_QUIT};
            push_event(&quit_event);
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return pop_event(event);
}

const char *const *Platform_Vulkan_GetInstanceExtensions(u32 *count)
{
    static const char *extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    };

    *count = 2;
    return extensions;
}

bool Platform_Vulkan_CreateSurface(platform_window_t *window, VkInstance instance,
                                   VkSurfaceKHR *surface)
{
    VkWin32SurfaceCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = s_instance,
        .hwnd = window->hwnd,
    };

    if (vkCreateWin32SurfaceKHR(instance, &create_info, NULL, surface) != VK_SUCCESS)
    {
        Log(ERROR, "Failed to create win32 vulkan surface");
        return false;
    }

    return true;
}
