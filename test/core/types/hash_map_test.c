#include <criterion/criterion.h>
#include <criterion/internal/assert.h>

#include <stdlib.h>

#include "core.h"
#include "hash_map.h"
#include "memory_arena.h"

#define SIZE 100000
#define BUCKET_COUNT 8192

u32 u32_keys[SIZE];
u64 u64_keys[SIZE];
u32 u32_vals[SIZE];
u64 u64_vals[SIZE];

void test_init()
{
     srand(1337);

     for (u64 i = 0; i < SIZE; i++)
     {
         u64_keys[i] = ((u64)i << 32) | (i + 1234);
         u32_keys[i] = i + 3456;
         u64_vals[i] = ((u64)(rand()) << 32) | rand();
         u32_vals[i] = (u32)u64_vals[i];
     }
}

Test(hash_map, basic_test_u32key)
{
    test_init();

    arena_t *arena = MemoryArena_Create("test_arena");
    cr_expect(arena);

    hash_map_t map = HashMap_Create(arena, BUCKET_COUNT);
    cr_expect(map.bucket_count == BUCKET_COUNT, "incorrect bucket count");
    cr_expect(map.size == 0, "incorrect size");

    /* Insert U32s */
    for (u64 i = 0; i < SIZE/2; i++)
    {
        cr_expect(HashMap_U32U32_Insert(&map, u32_keys[i], u32_vals[i]), "failed to insert");
    }
    for (u64 i = 0; i < SIZE/2; i++)
    {
        cr_expect(HashMap_U32_Remove(&map, u32_keys[i]), "failed to insert");
    }
    cr_expect(map.size == 0, "incorrect size");

    for (u64 i = 0; i < SIZE; i++)
    {
        cr_expect(HashMap_U32U32_Insert(&map, u32_keys[i], u32_vals[i]), "failed to insert");
    }
    cr_expect(HashMap_Size(&map) == SIZE);
    for (u64 i = 0; i < SIZE; i++)
    {
        u32 val;
        cr_expect(HashMap_U32U32_Get(&map, u32_keys[i], &val), "failed to get");
        cr_expect(val == u32_vals[i], "incorrect value");
    }
    for (u64 i = 0; i < SIZE; i++)
    {
        u32 val;
        cr_expect(HashMap_U32_Remove(&map, u32_keys[i]), "failed to remove");
        cr_expect(!HashMap_U32_Remove(&map, u32_keys[i]), "not removed");
        cr_expect(!HashMap_U32U32_Get(&map, u32_keys[i], &val), "failed to get");
    }
    cr_expect(HashMap_Size(&map) == 0);

    /* Insert U64s */
    for (u64 i = 0; i < SIZE/2; i++)
    {
        cr_expect(HashMap_U32U64_Insert(&map, u32_keys[i], u64_vals[i]), "failed to insert");
    }
    for (u64 i = 0; i < SIZE/2; i++)
    {
        cr_expect(HashMap_U32_Remove(&map, u32_keys[i]), "failed to insert");
    }
    cr_expect(map.size == 0, "incorrect size");

    for (u64 i = 0; i < SIZE; i++)
    {
        cr_expect(HashMap_U32U64_Insert(&map, u32_keys[i], u64_vals[i]), "failed to insert");
    }
    cr_expect(HashMap_Size(&map) == SIZE);
    for (u64 i = 0; i < SIZE; i++)
    {
        u64 val;
        cr_expect(HashMap_U32U64_Get(&map, u32_keys[i], &val), "failed to get");
        cr_expect(val == u64_vals[i], "incorrect value");
    }
    for (u64 i = 0; i < SIZE; i++)
    {
        u64 val;
        cr_expect(HashMap_U32_Remove(&map, u32_keys[i]), "failed to remove");
        cr_expect(!HashMap_U32_Remove(&map, u32_keys[i]), "not removed");
        cr_expect(!HashMap_U32U64_Get(&map, u32_keys[i], &val), "failed to get");
    }
    cr_expect(HashMap_Size(&map) == 0);

    /* Insert pointers */
    for (u64 i = 0; i < SIZE/2; i++)
    {
        cr_expect(HashMap_U32Ptr_Insert(&map, u32_keys[i], &u64_vals[i]), "failed to insert");
    }
    for (u64 i = 0; i < SIZE/2; i++)
    {
        cr_expect(HashMap_U32_Remove(&map, u32_keys[i]), "failed to insert");
    }
    cr_expect(map.size == 0, "incorrect size");

    for (u64 i = 0; i < SIZE; i++)
    {
        cr_expect(HashMap_U32Ptr_Insert(&map, u32_keys[i], &u64_vals[i]), "failed to insert");
    }
    cr_expect(HashMap_Size(&map) == SIZE);
    for (u64 i = 0; i < SIZE; i++)
    {
        cr_expect(HashMap_U32Ptr_Get(&map, u32_keys[i]) == &u64_vals[i], "incorrect value");
    }
    for (u64 i = 0; i < SIZE; i++)
    {
        cr_expect(HashMap_U32_Remove(&map, u32_keys[i]), "failed to remove");
        cr_expect(!HashMap_U32_Remove(&map, u32_keys[i]), "not removed");
        cr_expect(HashMap_U32Ptr_Get(&map, u32_keys[i]) == NULL, "failed to get");
    }
    cr_expect(HashMap_Size(&map) == 0);

    MemoryArena_Destroy(arena);
}

Test(hash_map, basic_test_u64key)
{
    test_init();

    arena_t *arena = MemoryArena_Create("test_arena");
    cr_expect(arena);

    hash_map_t map = HashMap_Create(arena, BUCKET_COUNT);
    cr_expect(map.bucket_count == BUCKET_COUNT, "incorrect bucket count");
    cr_expect(map.size == 0, "incorrect size");

    /* Insert U32s */
    for (u64 i = 0; i < SIZE/2; i++)
    {
        cr_expect(HashMap_U64U32_Insert(&map, u64_keys[i], u32_vals[i]), "failed to insert");
    }
    for (u64 i = 0; i < SIZE/2; i++)
    {
        cr_expect(HashMap_U64_Remove(&map, u64_keys[i]), "failed to insert");
    }
    cr_expect(map.size == 0, "incorrect size");

    for (u64 i = 0; i < SIZE; i++)
    {
        cr_expect(HashMap_U64U32_Insert(&map, u64_keys[i], u32_vals[i]), "failed to insert");
    }
    cr_expect(HashMap_Size(&map) == SIZE);
    for (u64 i = 0; i < SIZE; i++)
    {
        u32 val;
        cr_expect(HashMap_U64U32_Get(&map, u64_keys[i], &val), "failed to get");
        cr_expect(val == u32_vals[i], "incorrect value");
    }
    for (u64 i = 0; i < SIZE; i++)
    {
        u32 val;
        cr_expect(HashMap_U64_Remove(&map, u64_keys[i]), "failed to remove");
        cr_expect(!HashMap_U64_Remove(&map, u64_keys[i]), "not removed");
        cr_expect(!HashMap_U64U32_Get(&map, u64_keys[i], &val), "failed to get");
    }
    cr_expect(HashMap_Size(&map) == 0);

    /* Insert U64s */
    for (u64 i = 0; i < SIZE/2; i++)
    {
        cr_expect(HashMap_U64U64_Insert(&map, u64_keys[i], u64_vals[i]), "failed to insert");
    }
    for (u64 i = 0; i < SIZE/2; i++)
    {
        cr_expect(HashMap_U64_Remove(&map, u64_keys[i]), "failed to insert");
    }
    cr_expect(map.size == 0, "incorrect size");

    for (u64 i = 0; i < SIZE; i++)
    {
        cr_expect(HashMap_U64U64_Insert(&map, u64_keys[i], u64_vals[i]), "failed to insert");
    }
    cr_expect(HashMap_Size(&map) == SIZE);
    for (u64 i = 0; i < SIZE; i++)
    {
        u64 val;
        cr_expect(HashMap_U64U64_Get(&map, u64_keys[i], &val), "failed to get");
        cr_expect(val == u64_vals[i], "incorrect value");
    }
    for (u64 i = 0; i < SIZE; i++)
    {
        u64 val;
        cr_expect(HashMap_U64_Remove(&map, u64_keys[i]), "failed to remove");
        cr_expect(!HashMap_U64_Remove(&map, u64_keys[i]), "not removed");
        cr_expect(!HashMap_U64U64_Get(&map, u64_keys[i], &val), "failed to get");
    }
    cr_expect(HashMap_Size(&map) == 0);

    /* Insert pointers */
    for (u64 i = 0; i < SIZE/2; i++)
    {
        cr_expect(HashMap_U64Ptr_Insert(&map, u64_keys[i], &u64_vals[i]), "failed to insert");
    }
    for (u64 i = 0; i < SIZE/2; i++)
    {
        cr_expect(HashMap_U64_Remove(&map, u64_keys[i]), "failed to insert");
    }
    cr_expect(map.size == 0, "incorrect size");

    for (u64 i = 0; i < SIZE; i++)
    {
        cr_expect(HashMap_U64Ptr_Insert(&map, u64_keys[i], &u64_vals[i]), "failed to insert");
    }
    cr_expect(HashMap_Size(&map) == SIZE);
    for (u64 i = 0; i < SIZE; i++)
    {
        cr_expect(HashMap_U64Ptr_Get(&map, u64_keys[i]) == &u64_vals[i], "incorrect value");
    }
    for (u64 i = 0; i < SIZE; i++)
    {
        cr_expect(HashMap_U64_Remove(&map, u64_keys[i]), "failed to remove");
        cr_expect(!HashMap_U64_Remove(&map, u64_keys[i]), "not removed");
        cr_expect(HashMap_U64Ptr_Get(&map, u64_keys[i]) == NULL, "failed to get");
    }
    cr_expect(HashMap_Size(&map) == 0);

    MemoryArena_Destroy(arena);
}

Test(hash_map, basic_test_replace)
{
    arena_t *arena = MemoryArena_Create("test_arena");
    cr_expect(arena);

    hash_map_t map = HashMap_Create(arena, BUCKET_COUNT);

    cr_expect(map.bucket_count == BUCKET_COUNT, "incorrect bucket count");
    cr_expect(map.size == 0, "incorrect size");

    HashMap_U32U32_Insert(&map, 10, 20);
    HashMap_U32U32_Insert(&map, 10, 25);
    cr_expect(map.size == 1, "incorrect size");

    u32 val;
    cr_expect(HashMap_U32U32_Get(&map, 10, &val), "failed to get");
    cr_expect(val == 25, "incorrect value");

    map = HashMap_Create(arena, BUCKET_COUNT);

    cr_expect(map.bucket_count == BUCKET_COUNT, "incorrect bucket count");
    cr_expect(map.size == 0, "incorrect size");

    HashMap_U64U64_Insert(&map, U32_MAX + 10, U32_MAX + 20);
    HashMap_U64U64_Insert(&map, U32_MAX + 10, U32_MAX + 25);
    cr_expect(map.size == 1, "incorrect size");

    u64 val2;
    cr_expect(HashMap_U64U64_Get(&map, U32_MAX + 10, &val2), "failed to get");
    cr_expect(val2 == (U32_MAX + 25), "incorrect value");

    MemoryArena_Destroy(arena);
}

Test(hash_map, u64_large_keys_and_collisions)
{
    arena_t *arena = MemoryArena_Create("test_arena");
    cr_expect(arena);

    /* Use a single bucket to force collisions */
    hash_map_t map = HashMap_Create(arena, 1);
    cr_expect(map.bucket_count == 1, "incorrect bucket count");

    u64 keys[5] = {
        (1ULL << 63) | 0x1234,
        (1ULL << 62) | 0x5678,
        (1ULL << 61) | 0x9ABC,
        (1ULL << 60) | 0xDEF0,
        (1ULL << 59) | 0x1357,
    };
    u64 vals[5] = {
        0xAAAABBBBCCCCDDDDULL,
        0x1111222233334444ULL,
        0x5555666677778888ULL,
        0x9999AAAABBBBCCCCULL,
        0xDDDDEEEEFFFF0000ULL,
    };

    for (u64 i = 0; i < 5; i++)
    {
        cr_expect(HashMap_U64U64_Insert(&map, keys[i], vals[i]), "failed to insert");
    }
    cr_expect(HashMap_Size(&map) == 5, "incorrect size");

    for (u64 i = 0; i < 5; i++)
    {
        u64 out = 0;
        cr_expect(HashMap_U64U64_Get(&map, keys[i], &out), "failed to get");
        cr_expect(out == vals[i], "incorrect value");
    }

    /* Remove a middle key to exercise chain relinking */
    cr_expect(HashMap_U64_Remove(&map, keys[2]), "failed to remove");
    cr_expect(HashMap_Size(&map) == 4, "incorrect size");

    u64 out = 0;
    cr_expect(!HashMap_U64U64_Get(&map, keys[2], &out), "failed to remove");

    MemoryArena_Destroy(arena);
}
