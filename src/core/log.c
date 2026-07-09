#include <stdio.h>

#include "core_string.h"
#include "memory_arena.h"
#include "log.h"

#define LOG_CAPACITY 8192
#define MAX_ENTRY_LENGTH 80

static log_t *logger = NULL;

StaticAssert(IsPow2(LOG_CAPACITY), "bad capacity");

static const char *const severity_map[] =
    {
        [DEBUG] = "DEBUG",
        [INFO] = "INFO",
        [WARNING] = "WARNING",
        [ERROR] = "ERROR",
};

void Log_Init(arena_t *arena)
{
    if (logger)
        return;

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

    logger = l;
}

void Log(log_severity_t severity, const char *log, ...)
{
    if (!logger)
        return;

    va_list args;

    if (Log_Count() >= (logger->capacity - 1))
    {
        logger->head++;
        if (logger->head == logger->capacity)
            logger->head = 0;
    }

    log_entry_t *entry = &logger->entries[logger->tail];

    u64 pos = MemoryArena_Pos(logger->arena);

    va_start(args, log);
    string tmp = string_fmtv(logger->arena, log, args);
    va_end(args);
    string_copy(tmp, &entry->text);

    if (logger->stdout)
    {
        string stdout_string =
            string_fmt(logger->arena, "[%s] %s", severity_map[severity], tmp.str);
        printf("%s\n", stdout_string.str);
    }

    MemoryArena_PopTo(logger->arena, pos);

    entry->severity = severity;

    logger->tail++;
    if (logger->tail == logger->capacity)
        logger->tail = 0;
}

u64 Log_Count()
{
    return (logger->tail - logger->head) & logger->mask;
}

log_entry_t *Log_Get(u64 index)
{
    if (index >= Log_Count())
        return NULL;

    u64 i = logger->head + index;

    if (i >= logger->capacity)
    {
        i -= logger->capacity;
    }

    return &logger->entries[i];
}
