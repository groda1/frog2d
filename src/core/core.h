
#ifndef CORE_H
#define CORE_H


#include <stdint.h>
#include <stdbool.h>

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
#define KB(n)  (((u64)(n)) << 10)
#define MB(n)  (((u64)(n)) << 20)
#define GB(n)  (((u64)(n)) << 30)
#define TB(n)  (((u64)(n)) << 40)

// Min/Max/Clamp
#define Min(A,B) (((A)<(B))?(A):(B))
#define Max(A,B) (((A)>(B))?(A):(B))
#define ClampTop(A,X) Min(A,X)
#define ClampBot(X,B) Max(X,B)
#define Clamp(A,X,B) (((X)<(A))?(A):((X)>(B))?(B):(X))

// Alignment
#if defined(__clang__)
#define AlignOf(T) __alignof(T)
#elif defined(__GNUC__)
#define AlignOf(T) __alignof__(T)
#else
#error AlignOf not defined for this compiler.
#endif
#define AlignPow2(x, b) (((x) + (b) - 1) & (~((b) - 1)))

// Branch predictor hints
#define Expect(expr, val)       __builtin_expect((expr), (val))
#define Likely(expr)            Expect(expr,1)
#define Unlikely(expr)          Expect(expr,0)


// Attributes
#define AttributePacked         __attribute__((packed));

// Assert
#define StaticAssert            _Static_assert

#endif
