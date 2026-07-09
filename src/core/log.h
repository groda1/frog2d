#ifndef LOG_H
#define LOG_H

#include "core.h"
#include "memory_arena.h"

typedef enum
{
    DEBUG,
    INFO,
    WARNING,
    ERROR
} log_severity_t;

typedef struct
{
    log_severity_t severity;
    u8 text[256];
    

} log_entry_t;

typedef struct
{
    arena_t *arena;

    // Entry store
    log_entry_t *entries;
    u64 capacity;
    u64 mask;
    u64 head;
    u64 tail;

} log_t;

void Log_Init(arena_t *arena);


void Log_Entry();

#endif
