
#include "core.h"
#include "memory_arena.h"


typedef struct _hash_map_t hash_map_t;
typedef struct _hash_map_keyvalue_t hash_map_keyvalue_t;
typedef struct _hash_map_node_t hash_map_node_t;

struct _hash_map_t
{
    arena_t *arena;
    u64 size;
    u64 bucket_count;
    hash_map_node_t **buckets;
    hash_map_node_t *free_list;
};

hash_map_t HashMap_Create(arena_t *arena, u64 bucket_count);

bool HashMap_U32U32_Insert(hash_map_t *map, u32 key, u32 val);
bool HashMap_U64U32_Insert(hash_map_t *map, u64 key, u32 val);

bool HashMap_U32U64_Insert(hash_map_t *map, u32 key, u64 val);
bool HashMap_U64U64_Insert(hash_map_t *map, u64 key, u64 val);

bool HashMap_U32Ptr_Insert(hash_map_t *map, u32 key, void *val);
bool HashMap_U64Ptr_Insert(hash_map_t *map, u64 key, void *val);

bool HashMap_U32U32_Get(hash_map_t *map, u32 key, u32 *val_out);
bool HashMap_U64U32_Get(hash_map_t *map, u64 key, u32 *val_out);

bool HashMap_U32U64_Get(hash_map_t *map, u32 key, u64 *val_out);
bool HashMap_U64U64_Get(hash_map_t *map, u64 key, u64 *val_out);

void *HashMap_U32Ptr_Get(hash_map_t *map, u32 key);
void *HashMap_U64Ptr_Get(hash_map_t *map, u64 key);

bool HashMap_U32_Remove(hash_map_t *map, u32 key);
bool HashMap_U64_Remove(hash_map_t *map, u64 key);

u64 HashMap_Size(hash_map_t *map);
