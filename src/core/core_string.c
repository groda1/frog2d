#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "core.h"
#include "core_math.h"
#include "memory_arena.h"

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

// Length modifiers, so we know how wide a standard argument is when we advance
// the va_list past it (must match the C default-argument-promotion sizes).
typedef enum
{
    LM_NONE, LM_hh, LM_h, LM_l, LM_ll, LM_j, LM_z, LM_t, LM_L
} length_mod_t;

// snprintf-style append: copies n bytes into out (if there is room) and always
// returns the running total, so a NULL/short buffer measures instead of writing.
static u64 fmt_append(char *out, u64 cap, u64 written, const void *src, u64 n)
{
    if (out && written < cap)
    {
        u64 space = cap - written;
        MemoryCopy(out + written, src, n < space ? n : space);
    }
    return written + n;
}

static u64 fmt_pad(char *out, u64 cap, u64 written, u64 n)
{
    for (u64 i = 0; i < n; i++)
    {
        char space = ' ';
        written = fmt_append(out, cap, written, &space, 1);
    }
    return written;
}

static u64 fmt_f32(char *out, u64 cap, u64 written, f32 value, int prec)
{
    char *dst = (out && written < cap) ? out + written : NULL;
    u64 space = dst ? cap - written : 0;
    int n = snprintf(dst, space, "%.*f", prec, (double)value);
    return written + (n > 0 ? (u64)n : 0);
}

#define VEC_DEFAULT_PREC 3

// Walks `fmt`, forwarding every standard directive to vsnprintf and rendering
// the custom "%S" (a `string` by value) itself. Returns the length it would
// write in full (snprintf semantics); pass out=NULL,cap=0 to size.
static u64 string_format(char *out, u64 cap, const char *fmt, va_list args)
{
    u64 written = 0;
    const char *p = fmt;

    while (*p)
    {
        if (*p != '%')
        {
            const char *start = p;
            while (*p && *p != '%')
                p++;
            written = fmt_append(out, cap, written, start, (u64)(p - start));
            continue;
        }

        const char *spec = p; // points at '%'
        p++;

        if (*p == '%')
        {
            char pct = '%';
            written = fmt_append(out, cap, written, &pct, 1);
            p++;
            continue;
        }

        // flags (only '-' changes what we do; the rest we let vsnprintf handle)
        bool left = false;
        for (;;)
        {
            char c = *p;
            if (c == '-') left = true;
            else if (c != '+' && c != ' ' && c != '#' && c != '0') break;
            p++;
        }

        // width
        bool width_star = false;
        i64 width_val = 0;
        if (*p == '*') { width_star = true; p++; }
        else while (*p >= '0' && *p <= '9') { width_val = width_val * 10 + (*p - '0'); p++; }

        // precision
        bool has_prec = false, prec_star = false;
        i64 prec_val = 0;
        if (*p == '.')
        {
            has_prec = true;
            p++;
            if (*p == '*') { prec_star = true; p++; }
            else while (*p >= '0' && *p <= '9') { prec_val = prec_val * 10 + (*p - '0'); p++; }
        }

        // length modifier
        length_mod_t lm = LM_NONE;
        switch (*p)
        {
            case 'h': p++; if (*p == 'h') { p++; lm = LM_hh; } else lm = LM_h; break;
            case 'l': p++; if (*p == 'l') { p++; lm = LM_ll; } else lm = LM_l; break;
            case 'j': p++; lm = LM_j; break;
            case 'z': p++; lm = LM_z; break;
            case 't': p++; lm = LM_t; break;
            case 'L': p++; lm = LM_L; break;
            default: break;
        }

        char conv = *p;
        if (conv) p++;

        if (conv == 'S')
        {
            // custom: a `string` by value, sized by .len (slice-safe).
            i64 field = width_star ? va_arg(args, int) : width_val;
            i64 prec  = prec_star  ? va_arg(args, int) : (has_prec ? prec_val : -1);
            string s = va_arg(args, string);

            u64 n = s.len;
            if (prec >= 0 && (u64)prec < n)
                n = (u64)prec;

            bool la = left;
            if (field < 0) { la = true; field = -field; }
            u64 pad = ((u64)field > n) ? (u64)field - n : 0;

            if (!la) written = fmt_pad(out, cap, written, pad);
            written = fmt_append(out, cap, written, s.str, n);
            if (la)  written = fmt_pad(out, cap, written, pad);
            continue;
        }

        if (conv == 'v' && (*p >= '2' && *p <= '4'))
        {
            // custom: "%v3" -> a vec3 by value, printed as [x, y, z].
            // Precision picks the decimals ("%.1v3"); width is not applied.
            int dim = *p - '0';
            p++;

            if (width_star) (void)va_arg(args, int);
            i64 prec = prec_star ? va_arg(args, int)
                                 : (has_prec ? prec_val : VEC_DEFAULT_PREC);
            if (prec < 0) prec = VEC_DEFAULT_PREC;

            f32 c[4] = {0};
            switch (dim)
            {
                case 2: { vec2 v = va_arg(args, vec2); c[0] = v.X; c[1] = v.Y; } break;
                case 3: { vec3 v = va_arg(args, vec3); c[0] = v.X; c[1] = v.Y; c[2] = v.Z; } break;
                case 4: { vec4 v = va_arg(args, vec4); c[0] = v.X; c[1] = v.Y; c[2] = v.Z; c[3] = v.W; } break;
                default: break;
            }

            written = fmt_append(out, cap, written, "[", 1);
            for (int i = 0; i < dim; i++)
            {
                if (i) written = fmt_append(out, cap, written, ", ", 2);
                written = fmt_f32(out, cap, written, c[i], (int)prec);
            }
            written = fmt_append(out, cap, written, "]", 1);
            continue;
        }

        // standard directive: hand the exact slice back to vsnprintf, which
        // consumes its args from a copy positioned right here...
        char mini[64];
        u64 mlen = (u64)(p - spec);
        if (mlen >= sizeof(mini)) mlen = sizeof(mini) - 1;
        MemoryCopy(mini, spec, mlen);
        mini[mlen] = 0;

        char *dst = (out && written < cap) ? out + written : NULL;
        u64 space = dst ? cap - written : 0;

        va_list ap;
        va_copy(ap, args);
        int n = vsnprintf(dst, space, mini, ap);
        va_end(ap);
        if (n > 0) written += (u64)n;

        // ...then we advance the real va_list past exactly those same args.
        if (width_star) (void)va_arg(args, int);
        if (prec_star)  (void)va_arg(args, int);
        switch (conv)
        {
            case 'd': case 'i': case 'u': case 'o': case 'x': case 'X':
                switch (lm)
                {
                    case LM_ll: (void)va_arg(args, long long); break;
                    case LM_l:  (void)va_arg(args, long); break;
                    case LM_j:  (void)va_arg(args, intmax_t); break;
                    case LM_z:  (void)va_arg(args, size_t); break;
                    case LM_t:  (void)va_arg(args, ptrdiff_t); break;
                    default:    (void)va_arg(args, int); break; // hh/h/none promote to int
                }
                break;
            case 'e': case 'E': case 'f': case 'F': case 'g': case 'G': case 'a': case 'A':
                if (lm == LM_L) (void)va_arg(args, long double);
                else            (void)va_arg(args, double);
                break;
            case 'c':
                (void)va_arg(args, int); // char/wint_t both arrive as int
                break;
            case 's': case 'p': case 'n':
                (void)va_arg(args, void *);
                break;
            default:
                break; // unknown/empty conversion: no argument consumed
        }
    }

    return written;
}

string string_fmtv(arena_t *arena, const char *fmt, va_list args)
{
    va_list measure;
    va_copy(measure, args);
    u64 len = string_format(NULL, 0, fmt, measure);
    va_end(measure);

    string s;
    s.cap = len + 1;
    s.str = arena_push_array_no_zero(arena, u8, s.cap);

    va_list write;
    va_copy(write, args);
    string_format((char *)s.str, s.cap, fmt, write);
    va_end(write);

    s.str[len] = 0;
    s.len = len;
    return s;
}

string string_fmt_a(arena_t arena, string *s, ...);

string string_clone(arena_t *arena, string src)
{
    string new = string_new(arena, src.len + 1);
    string_copy(src, &new);
    return new;
}

bool string_match(string s1, string s2)
{
    if (s1.len != s2.len)
        return false;
    return MemoryMatch(s1.str, s2.str, s1.len);
}

void string_copy(string src, string *dst)
{
    Assert(dst);

    u64 len = Min(strlen((char *)src.str), dst->cap - 1);

    MemoryCopy(dst->str, src.str, len);
    dst->str[len] = 0;
    dst->len = len;
}
