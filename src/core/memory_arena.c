#include <sys/mman.h>

#include "log.h"
#include "memory_arena.h"

#define PAGE_SIZE ((u64)4096)

static void *os_reserve(u64 reserve_size);
static void os_release(void *ptr, u64 size);
static bool os_commit(void *ptr, u64 size);

static void *os_reserve(u64 reserve_size)
{
    // TODO: this is linux specific
    void *ptr = mmap(0, reserve_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED)
        return NULL;
    return ptr;
}

static void os_release(void *ptr, u64 size)
{
    // TODO: this is linux specific
    munmap(ptr, size);
}

static bool os_commit(void *ptr, u64 size)
{
    // TODO: this is linux specific
    if (mprotect(ptr, size, PROT_READ | PROT_WRITE))
        return true;
    return false;
}

arena_t *MemoryArena_Create(const char *name)
{
    return MemoryArena_CreateP(name,
                               (arena_params_t){
                                   .reserve_size = MEMORY_ARENA_DEFAULT_RESERVE_SIZE,
                                   .commit_size = MEMORY_ARENA_DEFAULT_COMMIT_SIZE,
                               });
}

arena_t *MemoryArena_CreateP(const char *name, arena_params_t params)
{
    u64 reserve_size = params.reserve_size;
    u64 commit_size = params.commit_size;

    reserve_size = AlignPow2(reserve_size, PAGE_SIZE);
    commit_size = AlignPow2(commit_size, PAGE_SIZE);

    void *base = os_reserve(reserve_size);

    if (base)
    {
        os_commit(base, commit_size);

        arena_t *arena = (arena_t *)base;
        arena->name = name;
        arena->current = arena;
        arena->commit_size = params.commit_size;
        arena->reserve_size = params.reserve_size;
        arena->base_pos = 0;
        arena->pos = ARENA_HEADER_SIZE;
        arena->commited = commit_size;
        arena->reserved = reserve_size;

        Log(DEBUG, "arena %s created (commited=%ju, reserved=%ju)\n", name, commit_size, reserve_size);
        return arena;
    }

    return NULL;
}

void MemoryArena_Destroy(arena_t *arena)
{
    for (arena_t *it = arena->current, *prev = 0; it != 0; it = prev)
    {
        prev = it->prev;
        os_release(it, it->reserved);
    }
}

void *MemoryArena_Push(arena_t *arena, u64 size, u64 align)
{
    arena_t *current = arena->current;
    u64 pos_pre = AlignPow2(current->pos, align);
    u64 pos_post = pos_pre + size;

    // Reserve new block if needed
    if (pos_post > current->reserved)
    {
        u64 reserve_size = arena->reserve_size;
        u64 commit_size = arena->commit_size;

        if (size + ARENA_HEADER_SIZE > reserve_size)
        {
            reserve_size = AlignPow2(size + ARENA_HEADER_SIZE, align);
            commit_size = AlignPow2(size + ARENA_HEADER_SIZE, align);
        }
        arena_t *new_block = MemoryArena_CreateP(current->name,
                                                 (arena_params_t){
                                                     .reserve_size = reserve_size,
                                                     .commit_size = commit_size});

        new_block->base_pos = current->base_pos + current->reserved;
        new_block->prev = arena->current;
        arena->current = new_block;

        current = new_block;
        pos_pre = AlignPow2(current->pos, align);
        pos_post = pos_pre + size;
    }

    // Commit new page in current block, if needed
    if (pos_post > current->commited)
    {
        u64 cmt_pst_aligned = pos_post + current->commit_size - 1;
        cmt_pst_aligned -= cmt_pst_aligned % current->commit_size;
        u64 cmt_pst_clamped = ClampTop(cmt_pst_aligned, current->reserved);
        u64 cmt_size = cmt_pst_clamped - current->commited;
        u8 *cmt_ptr = (u8 *)current + current->commited;

        os_commit(cmt_ptr, cmt_size);

        Log(DEBUG, "arena %s: commited %ju", cmt_size);
        current->commited = cmt_pst_clamped;
    }

    void *ret = 0;
    if (current->commited >= pos_post)
    {
        ret = (u8 *)current + pos_pre;
        current->pos = pos_post;
    }

    return ret;
}

void MemoryArena_Pop(arena_t *arena, u64 size)
{
    u64 pos = MemoryArena_Pos(arena);
    u64 pos_new = pos;

    if (pos > size)
    {
        pos_new = pos - size;
    }
    MemoryArena_PopTo(arena, pos_new);
}

void MemoryArena_PopTo(arena_t *arena, u64 pos)
{
    u64 big_pos = ClampBot(ARENA_HEADER_SIZE, pos);
    arena_t *current = arena->current;

    for (arena_t *prev = NULL; current->base_pos >= big_pos; current = prev)
    {
        prev = current->prev;
        os_release(current, current->reserved);
    }
    arena->current = current;

    u64 new_pos = big_pos - current->base_pos;
    current->pos = new_pos;
}

u64 MemoryArena_Pos(arena_t *arena)
{
    arena_t *current = arena->current;
    u64 pos = current->base_pos + current->pos;
    return pos;
}

void MemoryArena_Clear(arena_t *arena)
{
    MemoryArena_PopTo(arena, 0);
}

void MemoryArena_Print(arena_t *arena)
{
    arena_t * current = arena->current;

    Log(DEBUG, "%s:", current->name);
    while (current)
    {
        Log(DEBUG, "  [reserved=%juKB commit_size=%juKB base_pos=%ju] commited=%juKB pos=%ju",
            current->reserved / KB(1),
            current->commit_size / KB(1),
            current->base_pos,
            current->commited / KB(1),
            current->pos);

        current = current->prev;
    }
}
