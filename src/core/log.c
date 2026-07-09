#include <stdio.h>

#include "core_string.h"
#include "memory_arena.h"
#include "log.h"

#define LOG_CAPACITY 8192
#define MAX_ENTRY_LENGTH 80

static log_t *s_logger = NULL;

StaticAssert(IsPow2(LOG_CAPACITY), "bad capacity");

static const char *const severity_map[] =
    {
        [DEBUG] = "DEBUG",
        [INFO] = "INFO",
        [WARNING] = "WARNING",
        [ERROR] = "ERROR",
};

void Log_Init(void)
{
    if (s_logger)
        return;

    arena_t *arena = MemoryArena_Create("log-arena");
    log_t *l = arena_push(arena, log_t);

    l->arena = arena;
    l->stdout = true;

    l->capacity = LOG_CAPACITY;
    l->head = 0;
    l->tail = 0;
    l->mask = LOG_CAPACITY - 1;

    l->entries = arena_push_array(arena, log_entry_t, LOG_CAPACITY);

    for (u32 i = 0; i < LOG_CAPACITY; i++)
    {
        l->entries[i].text = string_new(arena, MAX_ENTRY_LENGTH);
    }

    s_logger = l;
}

void Log_Destroy(void)
{
    if (!s_logger)
        return;

    MemoryArena_Print(s_logger->arena);
    MemoryArena_Destroy(s_logger->arena);
    s_logger = NULL;
}

void Log(log_severity_t severity, const char *log, ...)
{
    if (!s_logger)
        return;

    va_list args;

    if (Log_Count() >= (s_logger->capacity - 1))
    {
        s_logger->head++;
        if (s_logger->head == s_logger->capacity)
            s_logger->head = 0;
    }

    log_entry_t *entry = &s_logger->entries[s_logger->tail];

    u64 pos = MemoryArena_Pos(s_logger->arena);

    va_start(args, log);
    string tmp = string_fmtv(s_logger->arena, log, args);
    va_end(args);
    string_copy(tmp, &entry->text);

    if (s_logger->stdout)
    {
        string stdout_string =
            string_fmt(s_logger->arena, "[%s] %s", severity_map[severity], tmp.str);
        printf("%s\n", stdout_string.str);
    }

    MemoryArena_PopTo(s_logger->arena, pos);

    entry->severity = severity;

    s_logger->tail++;
    if (s_logger->tail == s_logger->capacity)
        s_logger->tail = 0;
}

u64 Log_Count()
{
    return (s_logger->tail - s_logger->head) & s_logger->mask;
}

log_entry_t *Log_Get(u64 index)
{
    if (index >= Log_Count())
        return NULL;

    u64 i = s_logger->head + index;

    if (i >= s_logger->capacity)
    {
        i -= s_logger->capacity;
    }

    return &s_logger->entries[i];
}
