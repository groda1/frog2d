#include <stdio.h>

#include "core_string.h"

string string_new(arena_t *arena, u64 capacity)
{
    return (string){
        .str = arena_push_array(arena, u8, capacity),
        .len = 0,
        .cap = capacity,
    };
}

string string_from_l(const char *str, u64 len)
{
    return (string){
        .str = (u8 *)str,
        .len = len,
        .cap = 0,
    };
}

string string_from(const char *str)
{
    return (string){
        .str = (u8 *)str,
        .len = strlen(str),
        .cap = 0,
    };
}

string string_fmt(arena_t *arena, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    string s = string_fmtv(arena, fmt, args);
    va_end(args);
    return s;
}

string string_fmtv(arena_t *arena, const char *fmt, va_list args)
{
    va_list args2;
    va_copy(args2, args);

    u32 capacity = vsnprintf(0, 0, fmt, args) + 1;

    string s;
    s.str = arena_push_array_no_zero(arena, u8, capacity);
    s.len = vsnprintf((char *)s.str, capacity, fmt, args2);
    s.cap = capacity;

    va_end(args2);
    return s;
}

string string_fmt_a(arena_t arena, string *s, ...);
string string_clone(arena_t *arena, string src);

bool string_match(string s1, string s2)
{
    if (s1.len != s2.len)
        return false;
    return MemoryMatch(s1.str, s2.str, s1.len);
}

void string_copy(string src, string *dst)
{
    u64 len = Min(strlen((char *)src.str), dst->cap - 1);

    MemoryCopy(dst->str, src.str, len);
    dst->str[len] = 0;
    dst->len = len;
}
