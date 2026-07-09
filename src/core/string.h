
#include "core.h"
#include "memory_arena.h"


typedef struct
{
    u8 *str;
    u64 len;
    u64 cap;
} string;


string string_new(arena_t *arena, u64 capacity);
string string_from(arena_t *arena, const char *str);

void string_fmt(string *s, const char *str, ...);

string string_fmt_a(arena_t arena, string *s, ...);


string string_clone(arena_t *arena, string src);

