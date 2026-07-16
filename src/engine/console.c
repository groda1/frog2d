
#include "platform.h"
#include "render_types.h"
#include "renderer.h"
#include "draw.h"

#define TOGGLE_SPEED        7.5f
#define CARET_BLINK_SPEED   1.5f
#define SCROLL_LINES        15

typedef struct
{
    bool active;




} console_t;

bool Console_Init()
{
    return true;
}

void Console_Destroy()
{

}

void Console_Update(u32 delta_time)
{
    (void)delta_time;
}

void Console_Draw()
{
    window_extent_t extent = Renderer_GetWindowExtent();

    //u32 scroll_offset = 0;

    u32 console_height = (extent.height / 4) * 3;
    u32 console_offset = extent.height - console_height;

    Draw_Quad(0, console_offset, extent.width, console_height, V4(0.0, 0.0, 0.0, 0.9));
}

key_handle_result_t Console_HandleKeyDown(key_code_t key)
{
    (void)key;
    return KEY_EVENT_PASSTHROUGH;
}

key_handle_result_t Console_HandleKeyUp(key_code_t key)
{
    (void)key;

    return KEY_EVENT_PASSTHROUGH;
}
