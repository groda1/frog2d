#include "hash_map.h"
#include "list.h"
#include "core.h"
#include "memory_arena.h"

#include "xxh3.h"


StaticAssert(sizeof(u64) == sizeof(XXH64_hash_t));

struct _hash_map_keyvalue_t
{
    union
    {
        u64 key_u64;
    };
    union
    {
        u32 val_u32;
        u64 val_u64;
        void *val_ptr;
    };
};

struct _hash_map_node_t
{
    hash_map_node_t *next;
    hash_map_keyvalue_t keyvalue;
};


static hash_map_node_t *get_new_node(hash_map_t *map)
{
    Assert(map);
    Assert(map->arena);

    hash_map_node_t *node = map->free_list;
    if (node)
        SLLPopFirst(map->free_list, next);
    else
        node = arena_push(map->arena, hash_map_node_t);
    MemoryZeroItem(node);
    Assert(node);
    return node;
}

static inline bool hash_map_insert(hash_map_t *map, hash_map_keyvalue_t keyval, u64 hash)
{
    Assert(map);

    u64 bucket = hash & (map->bucket_count - 1);
    hash_map_node_t *node = map->buckets[bucket];
    while (node)
    {
        if (node->keyvalue.key_u64 == keyval.key_u64)
            break;
        node = node->next;
    }

    if (!node)
    {
        node = get_new_node(map);
        SLLInsertFirst(map->buckets[bucket], next, node);
        map->size++;
    }

    node->keyvalue = keyval;

    return true;
}

static inline bool hash_map_get_u64(hash_map_t *map, u64 key, u64 hash, void *out, u32 out_size)
{
    StaticAssert(sizeof(u64) == sizeof(XXH64_hash_t));
    Assert(map);
    Assert(out);

    u64 bucket = hash & (map->bucket_count - 1);

    hash_map_node_t *node = map->buckets[bucket];
    while (node)
    {
        if (node->keyvalue.key_u64 == key)
        {
            MemoryCopy(out, &node->keyvalue.val_u64, out_size);
            return true;
        }
        node = node->next;
    }

    return false;
}

static inline bool hash_map_remove_u64(hash_map_t *map, u64 key, u64 hash)
{
    Assert(map);

    u64 bucket = hash & (map->bucket_count - 1);

    hash_map_node_t *node = map->buckets[bucket];
    hash_map_node_t *removed = NULL;

    if (node)
    {
        if (node->keyvalue.key_u64 == key)
        {
            removed = node;
            map->buckets[bucket] = node->next;
            goto removed;
        }
        while (node->next)
        {
            if (node->next->keyvalue.key_u64 == key)
            {
                removed = node->next;
                node->next = node->next->next;
                goto removed;
            }
            node = node->next;
        }
    }
    return false;

removed:
    SLLInsertFirst(map->free_list, next, removed);
    map->size--;
    return true;
}

hash_map_t HashMap_Create(arena_t *arena, u64 bucket_count)
{
    Assert(arena);
    AssertAlways(IsPow2(bucket_count));

    hash_map_node_t **buckets = arena_push_array(arena, hash_map_node_t*, bucket_count);
    return (hash_map_t){
        .size = 0,
        .bucket_count = bucket_count,
        .buckets = buckets,
        .free_list = NULL,
        .arena = arena,
    };
}

bool HashMap_U32U32_Insert(hash_map_t *map, u32 key, u32 val)
{
    hash_map_keyvalue_t keyvalue = {.key_u64 = key, .val_u32 = val};
    return hash_map_insert(map, keyvalue, XXH3_64bits(&key, sizeof(key)));
}
bool HashMap_U64U32_Insert(hash_map_t *map, u64 key, u32 val)
{
    hash_map_keyvalue_t keyvalue = {.key_u64 = key, .val_u32 = val};
    return hash_map_insert(map, keyvalue, XXH3_64bits(&key, sizeof(key)));
}
bool HashMap_U32U64_Insert(hash_map_t *map, u32 key, u64 val)
{
    hash_map_keyvalue_t keyvalue = { .key_u64 = key, .val_u64 = val};
    return hash_map_insert(map, keyvalue, XXH3_64bits(&key, sizeof(key)));
}
bool HashMap_U64U64_Insert(hash_map_t *map, u64 key, u64 val)
{
    hash_map_keyvalue_t keyvalue = { .key_u64 = key, .val_u64 = val};
    return hash_map_insert(map, keyvalue, XXH3_64bits(&key, sizeof(key)));
}
bool HashMap_U32Ptr_Insert(hash_map_t *map, u32 key, void *val)
{
    hash_map_keyvalue_t keyvalue = { .key_u64 = key, .val_ptr = val};
    return hash_map_insert(map, keyvalue, XXH3_64bits(&key, sizeof(key)));
}
bool HashMap_U64Ptr_Insert(hash_map_t *map, u64 key, void *val)
{
    hash_map_keyvalue_t keyvalue = { .key_u64 = key, .val_ptr = val};
    return hash_map_insert(map, keyvalue, XXH3_64bits(&key, sizeof(key)));
}

bool HashMap_U32U32_Get(hash_map_t *map, u32 key, u32 *val_out)
{
    return hash_map_get_u64(map, key, XXH3_64bits(&key, sizeof(key)), val_out, sizeof(*val_out));
}
bool HashMap_U64U32_Get(hash_map_t *map, u64 key, u32 *val_out)
{
    return hash_map_get_u64(map, key, XXH3_64bits(&key, sizeof(key)), val_out, sizeof(*val_out));
}
bool HashMap_U32U64_Get(hash_map_t *map, u32 key, u64 *val_out)
{
    return hash_map_get_u64(map, key, XXH3_64bits(&key, sizeof(key)), val_out, sizeof(*val_out));
}
bool HashMap_U64U64_Get(hash_map_t *map, u64 key, u64 *val_out)
{
    return hash_map_get_u64(map, key, XXH3_64bits(&key, sizeof(key)), val_out, sizeof(*val_out));
}
void *HashMap_U32Ptr_Get(hash_map_t *map, u32 key)
{
    void *val_out = NULL;
    hash_map_get_u64(map, key, XXH3_64bits(&key, sizeof(key)), &val_out, sizeof(val_out));
    return val_out;
}
void *HashMap_U64Ptr_Get(hash_map_t *map, u64 key)
{
    void *val_out = NULL;
    hash_map_get_u64(map, key, XXH3_64bits(&key, sizeof(key)), &val_out, sizeof(val_out));
    return val_out;
}

bool HashMap_U32_Remove(hash_map_t *map, u32 key)
{
    return hash_map_remove_u64(map, key, XXH3_64bits(&key, sizeof(key)));
}
bool HashMap_U64_Remove(hash_map_t *map, u64 key)
{
    return hash_map_remove_u64(map, key, XXH3_64bits(&key, sizeof(key)));
}

u64 HashMap_Size(hash_map_t *map)
{
    Assert(map);
    return map->size;
}
