
#ifndef STRINGS_H
#define STRINGS_H

#include "core.h"
#include "memory_arena.h"

#define NullString ((string){0})

typedef struct
{
    u8 *str;
    u64 len;
    u64 cap;
} string;

#define string_lit(S)  string_from((U8*)(S), sizeof(S) - 1)

string string_new(arena_t *arena, u64 capacity);
string string_from(const char *str, u64 len);

string string_fmt(arena_t *arena, const char *fmt, ...);
string string_fmtv(arena_t *arena, const char *fmt, va_list args);

string string_fmt_a(arena_t arena, string *s, ...);


string string_clone(arena_t *arena, const string *src);
void string_copy(const string *src, string *dst);

#endif
