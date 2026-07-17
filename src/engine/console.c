
#include "core.h"
#include "core_math.h"
#include "log.h"
#include "platform.h"
#include "render_types.h"
#include "renderer.h"
#include "draw.h"

#define TOGGLE_SPEED        5.0f
#define CARET_BLINK_SPEED   1.5f
#define SCROLL_LINES        15

static void toggle_console();

typedef struct
{
    bool active;

    f32 scroll;


} console_t;

static console_t s_console = {};

bool Console_Init()
{
    return true;
}

void Console_Destroy()
{

}

void Console_Update(f32 delta_time)
{
    f32 scroll = s_console.scroll;

    if (s_console.active && scroll < 1.0f)
    {
        scroll += delta_time * TOGGLE_SPEED;
        s_console.scroll = ClampTop(scroll, 1.0f);
    }
    else if (!s_console.active && scroll > 0.0f)
    {
        scroll -= delta_time * TOGGLE_SPEED;
        s_console.scroll = ClampBot(0.0f, scroll);
    }
}

void Console_Draw()
{
    if (s_console.scroll > 0.0f)
    {
        window_extent_t extent = Renderer_GetWindowExtent();

        u32 console_height = (extent.height / 4) * 3;
        u32 console_offset = extent.height - console_height;

        u32 current_scroll = (u32)lerp((f32)console_height, smoothstep(s_console.scroll), 0.0f);

        Draw_Quad(0, console_offset + current_scroll, extent.width, console_height, V4(0.0f, 0.0f, 0.0f, 0.9f));
    }
}

key_handle_result_t Console_HandleKeyDown(key_code_t key)
{

    switch (key)
    {
    case KEY_F1:
        toggle_console();
        return KEY_EVENT_CONSUMED;
    default:
        return KEY_EVENT_PASSTHROUGH;
    }
    return KEY_EVENT_PASSTHROUGH;
}

key_handle_result_t Console_HandleKeyUp(AttributeMaybeUnused key_code_t key)
{
    return KEY_EVENT_PASSTHROUGH;
}

static void toggle_console()
{
    Log(DEBUG, "toggle console");
    s_console.active = !s_console.active;
}
