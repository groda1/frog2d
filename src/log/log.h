#ifndef LOG_H
#define LOG_H

#include "core.h"
#include "core_string.h"
#include "memory_arena.h"

typedef enum
{
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
} log_severity_t;

typedef struct
{
    log_severity_t severity;
    string text;
} log_entry_t;

typedef struct
{
    arena_t *arena;
    bool stdout;

    // Entry store
    log_entry_t *entries;
    u64 capacity;
    u64 mask;
    u64 head;
    u64 tail;

} log_t;

void Log_Init(arena_t *arena);


void Log(log_severity_t severity, const char *log, ...);

u64 Log_Count();
log_entry_t *Log_Get(u64 index);


#endif
