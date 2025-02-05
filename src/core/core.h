
#ifndef CORE_H
#define CORE_H


#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>

// Types
typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;
typedef int8_t      s8;
typedef int16_t     s16;
typedef int32_t     s32;
typedef int64_t     s64;
typedef float       f32;
typedef double      f64;

// Constants
#define U8_MAX  UINT8_MAX
#define U32_MAX UINT32_MAX
#define U64_MAX UINT64_MAX

// Units
#define KB(n) (((u64)(n)) << 10)
#define MB(n) (((u64)(n)) << 20)
#define GB(n) (((u64)(n)) << 30)
#define TB(n) (((u64)(n)) << 40)

// Min/Max/Clamp
#define Min(A, B) (((A) < (B)) ? (A) : (B))
#define Max(A, B) (((A) > (B)) ? (A) : (B))
#define ClampTop(A, X) Min(A, X)
#define ClampBot(X, B) Max(X, B)
#define Clamp(A, X, B) (((X) < (A)) ? (A) : ((X) > (B)) ? (B) \
                                                        : (X))
// Alignment
#if defined(__clang__)
#define AlignOf(T) __alignof(T)
#elif defined(__GNUC__)
#define AlignOf(T) __alignof__(T)
#else
#error AlignOf not defined for this compiler.
#endif
#define AlignPow2(x, b) (((x) + (b) - 1) & (~((b) - 1)))
#define IsPow2(x) ((x) != 0 && ((x) & ((x) - 1)) == 0)


// Branch predictor hints
#define Expect(expr, val)       __builtin_expect((expr), (val))
#define Likely(expr)            Expect(expr, 1)
#define Unlikely(expr)          Expect(expr, 0)

// Attributes
#define AttributePacked         __attribute__((packed));

// Assert
#define StaticAssert            _Static_assert


// Memory functions
#define MemoryCopy(dst, src, size)      memcpy((dst), (src), (size))
#define MemoryMove(dst, src, size)      memmove((dst), (src), (size))
#define MemorySet(dst, byte, size)      memset((dst), (byte), (size))
#define MemoryCompare(a, b, size)       memcmp((a), (b), (size))
#define MemoryStrlen(ptr)               strlen(ptr)

#define MemoryZero(dst, size)           MemorySet((dst), 0, (size))
#define MemoryZeroStruct(s)             MemoryZero((s), sizeof(*(s)))
#define MemoryZeroArray(a)              MemoryZero((a), sizeof(a))

#define MemoryMatch(a, b, z) (MemoryCompare((a), (b), (z)) == 0)

#endif
