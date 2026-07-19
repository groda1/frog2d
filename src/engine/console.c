
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

typedef struct
{
    bool active;

    f32 window_scroll;

    i64 line_scroll;
    u64 line_scroll_updated_at_log_count;

} console_t;

typedef struct
{
    u32 width;
    u32 height;
    u32 bottom;
    u32 lines;
} console_layout_t;

static void toggle_console();
static console_layout_t layout();

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
        // If log count has changed we need to adjust the line scroll or else the scroll wont
        // be fixed in place.
        if (log_count > s_console.line_scroll_updated_at_log_count)
        {
            s_console.line_scroll += log_count - s_console.line_scroll_updated_at_log_count;
            s_console.line_scroll_updated_at_log_count = log_count;
        }
    }
}

void Console_Draw()
{
    if (s_console.window_scroll <= 0.0f)
        return;

    console_layout_t l = layout();
    u64 log_count = Log_Count();

    // Draw console bg
    Draw_Quad(0, l.bottom, l.width, l.height, COLOR_BG);

    if (log_count > 0)
    {
        // Draw scrollbar
        u32 track = l.lines * CONSOLE_TEXT_HEIGHT;
        u32 scrollbar_height = track * Min(l.lines, log_count) / log_count;
        u32 scrollbar_offset = track * (u64)s_console.line_scroll / log_count;
        Draw_Quad(l.width - SCROLLBAR_WIDTH - 8, l.bottom + CONSOLE_TEXT_HEIGHT + scrollbar_offset,
                  SCROLLBAR_WIDTH, scrollbar_height, COLOR_BG_SCROLLBAR);

        // Draw console lines
        Draw_SetTextSize(CONSOLE_TEXT_WIDTH);
        u64 line_count = Min(l.lines, log_count - (u64)s_console.line_scroll);
        for (u32 i = 0; i < line_count; i++)
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
                    prefix = string_lit("[unknown]");
                    prefix_color = COLOR_TEXT_ERROR;
            }

            Draw_SetTextColor(prefix_color);
            Draw_Text(X_OFFSET, l.bottom + ((i + 1) * CONSOLE_TEXT_HEIGHT), prefix);
            Draw_SetTextColor(COLOR_TEXT);
            Draw_Text(X_OFFSET + 152, l.bottom + ((i + 1) * CONSOLE_TEXT_HEIGHT), log->text);
        }
    }

    // TODO draw input prompt
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
                console_layout_t l = layout();
                i64 log_count = Log_Count();

                s_console.line_scroll += SCROLL_LINES;
                if ((s_console.line_scroll + l.lines) > log_count)
                    s_console.line_scroll = ClampBot(0, log_count - l.lines);
                s_console.line_scroll_updated_at_log_count = log_count;

                return KEY_EVENT_CONSUMED;
            }
            case KEY_PGDN:
            {
                i64 log_count = Log_Count();
                s_console.line_scroll = ClampBot(0, s_console.line_scroll - SCROLL_LINES);
                s_console.line_scroll_updated_at_log_count = log_count;

                return KEY_EVENT_CONSUMED;
            }
            default:
            {
                Log(DEBUG, "console undhandled keycode: %u", key);
                return KEY_EVENT_CONSUMED;
            }
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
    s_console.active = !s_console.active;
    s_console.line_scroll = 0;
}

static console_layout_t layout()
{
    window_extent_t extent = Renderer_GetWindowExtent();

    console_layout_t l;
    l.width = extent.width;
    l.height = (extent.height / 4) * 3;
    l.lines = (l.height / CONSOLE_TEXT_HEIGHT) - 1;

    u32 visible = (u32)(smoothstep(s_console.window_scroll) * (f32)l.height);
    l.bottom = extent.height - visible;

    return l;
}
