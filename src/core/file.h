#ifndef FILE_H
#define FILE_H

#include "core.h"
#include "memory_arena.h"

u8 *File_Read(arena_t *arena, const char *path, u64 *size_out);

#endif
