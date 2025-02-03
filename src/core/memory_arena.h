
#ifndef MEMORY_ARENA_H
#define MEMORY_ARENA_H

#include "core.h"

#define ARENA_HEADER_SIZE 128

// #define push_array_no_zero_aligned(a, T, c, align) (T *)arena_push((a), sizeof(T)*(c), (align))
// #define push_array_aligned(a, T, c, align) (T *)MemoryZero(push_array_no_zero_aligned(a, T, c, align), sizeof(T)*(c))
// #define push_array_no_zero(a, T, c) push_array_no_zero_aligned(a, T, c, Max(8, AlignOf(T)))
// #define push_array(a, T, c) push_array_aligned(a, T, c, Max(8, AlignOf(T)))


#define MEMORY_ARENA_DEFAULT_RESERVE_SIZE MB(64)
#define MEMORY_ARENA_DEFAULT_COMMIT_SIZE KB(64)

typedef struct
{
    u64 reserve_size;
    u64 commit_size;
} arena_params_t;

typedef struct arena_t arena_t;
struct arena_t
{
    const char *name;
    arena_t *prev;    // previous arena in chain
    arena_t *current; // current arena in chain
    u64 commit_size;
    u64 reserve_size;
    u64 base_pos;
    u64 pos;
    u64 commited;
    u64 reserved;
};

StaticAssert(sizeof(arena_t) <= ARENA_HEADER_SIZE, "arena_t larger than arena header size");

arena_t *MemoryArena_Create(const char *name);
arena_t *MemoryArena_CreateP(const char *name, arena_params_t params);
void MemoryArena_Destroy(arena_t *arena);

void *MemoryArena_Push(arena_t *arena, u64 size, u64 align);
void MemoryArena_Pop(arena_t *arena, u64 size);
u64 MemoryArena_Pos(arena_t *arena);
void MemoryArena_PopTo(arena_t *arena, u64 pos);

void MemoryArena_Clear(arena_t *arena);

#endif