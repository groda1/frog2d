
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

#define CONSOLE_TEXT_WIDTH  16
#define CONSOLE_TEXT_HEIGHT (CONSOLE_TEXT_WIDTH * 2)

#define COLOR_INPUT_TEXT    V4(1.0f, 1.0f, 1.0f, 1.0f)
#define COLOR_TEXT          V4(0.7f, 0.7f, 0.8f, 1.0f)
#define COLOR_TEXT_DEBUG    V4(0.3f, 0.9f, 0.9f, 1.0f)
#define COLOR_TEXT_INFO     V4(0.3f, 0.9f, 0.3f, 1.0f)
#define COLOR_TEXT_WARNING  V4(0.9f, 0.4f, 0.3f, 1.0f)
#define COLOR_TEXT_ERROR    V4(0.9f, 0.3f, 0.3f, 1.0f)
#define COLOR_TEXT_CVAR     V4(0.3f, 0.3f, 0.9f, 1.0f)

#define COLOR_BG            V4(0.0f, 0.0f, 0.0f, 0.975f)
#define COLOR_BG_SCROLLBAR  V4(0.5f, 0.5f, 0.5f, 0.975f)

#define X_OFFSET    8

#define SCROLLBAR_WIDTH 16

static void toggle_console();

typedef struct
{
    bool active;

    f32 window_scroll;

    i64 line_scroll;
    u64 line_scroll_updated_at_log_count;

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
    f32 scroll = s_console.window_scroll;

    if (s_console.active && scroll < 1.0f)
    {
        scroll += delta_time * TOGGLE_SPEED;
        s_console.window_scroll = ClampTop(scroll, 1.0f);
    }
    else if (!s_console.active && scroll > 0.0f)
    {
        scroll -= delta_time * TOGGLE_SPEED;
        s_console.window_scroll = ClampBot(0.0f, scroll);
    }

    if (s_console.active && s_console.line_scroll)
    {
        u64 log_count = Log_Count();
        if (log_count > s_console.line_scroll_updated_at_log_count)
        {
            s_console.line_scroll += log_count - s_console.line_scroll_updated_at_log_count;
            s_console.line_scroll_updated_at_log_count = log_count;
        }
    }
}

void Console_Draw()
{
    if (s_console.window_scroll > 0.0f)
    {
        window_extent_t extent = Renderer_GetWindowExtent();

        u32 console_height = (extent.height / 4) * 3;
        u32 console_offset = extent.height - console_height;
        u32 current_scroll = (u32)lerp((f32)console_height, smoothstep(s_console.window_scroll), 0.0f);
        u32 console_lines = (console_height / CONSOLE_TEXT_HEIGHT) - 1;
        u64 log_count = Log_Count();

        // Draw console bg
        Draw_Quad(0, console_offset + current_scroll, extent.width, console_height, COLOR_BG);

        // Draw scrollbar
        u32 scrollbar_height =
            (ClampTop((f32)console_lines / (f32)log_count, 1.0f)) * (console_lines * CONSOLE_TEXT_HEIGHT);
        u32 scrollbar_offset =
            ClampBot(0, ((f32)s_console.line_scroll / (f32)log_count) * (console_lines * CONSOLE_TEXT_HEIGHT));
        Draw_Quad(extent.width - SCROLLBAR_WIDTH - 8,
                  console_offset + current_scroll + CONSOLE_TEXT_HEIGHT + scrollbar_offset, SCROLLBAR_WIDTH,
                  scrollbar_height, COLOR_BG_SCROLLBAR);

        // Draw console lines
        Draw_SetTextSize(CONSOLE_TEXT_WIDTH);
        for (u32 i = 0; i < Min(console_lines, log_count); i++)
        {
            u64 log_index = i + s_console.line_scroll;

            log_entry_t *log = Log_Get(log_index);
            if (!log)
            {
                Log(ERROR, "bug! scroll log out of bounds");
                continue;
            }
            string prefix;
            vec4 prefix_color;

            switch (log->severity)
            {
                case DEBUG:
                    prefix = string_lit("[debug]");
                    prefix_color = COLOR_TEXT_DEBUG;
                    break;
                case INFO:
                    prefix = string_lit("[info]");
                    prefix_color = COLOR_TEXT_INFO;
                    break;
                case WARNING:
                    prefix = string_lit("[warning]");
                    prefix_color = COLOR_TEXT_WARNING;
                    break;
                case ERROR:
                    prefix = string_lit("[error]");
                    prefix_color = COLOR_TEXT_ERROR;
                    break;
                case CVAR:
                    prefix = string_lit("[cvar]");
                    prefix_color = COLOR_TEXT_CVAR;
                    break;
                default:
            }

            Draw_SetTextColor(prefix_color);
            Draw_Text(X_OFFSET, console_offset + current_scroll + ((i + 1) * CONSOLE_TEXT_HEIGHT), prefix);
            Draw_SetTextColor(COLOR_TEXT);
            Draw_Text(X_OFFSET + 152, console_offset + current_scroll + ((i + 1) * CONSOLE_TEXT_HEIGHT), log->text);
        }
    }
}

key_handle_result_t Console_HandleKeyDown(key_code_t key)
{
    if (key == KEY_F1)
    {
        toggle_console();
        return KEY_EVENT_CONSUMED;
    }

    if (s_console.active)
    {
        switch (key)
        {
            case KEY_PGUP:
            {
                window_extent_t extent = Renderer_GetWindowExtent();

                u32 console_height = (extent.height / 4) * 3;
                u32 console_lines = (console_height / CONSOLE_TEXT_HEIGHT) - 1;

                i64 log_count = Log_Count();

                s_console.line_scroll += SCROLL_LINES;
                if ((s_console.line_scroll + console_lines) > (i64)log_count)
                    s_console.line_scroll = ClampBot(0, log_count - console_lines);
                s_console.line_scroll_updated_at_log_count = log_count;
                break;
            }
            case KEY_PGDN:
            {
                s_console.line_scroll = ClampBot(0, s_console.line_scroll - SCROLL_LINES);
                break;
            }
            default: break;

        }
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
    s_console.line_scroll = 0;
}
