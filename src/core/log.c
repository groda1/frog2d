#include "log.h"

#define DEFAULT_CAPACITY 8192

static log_t *logger = NULL;


StaticAssert(IsPow2(DEFAULT_CAPACITY), "bad capacity");

void Log_Init(arena_t *arena)
{
    log_t *l = arena_push(arena, log_t);

    l->arena = arena;
    l->capacity = DEFAULT_CAPACITY;

    l->entries = arena_push_array(arena, log_entry_t, DEFAULT_CAPACITY);




    logger = l;

}