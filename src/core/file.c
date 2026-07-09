#include <stdio.h>

#include "file.h"
#include "log.h"

u8 *File_Read(arena_t *arena, const char *path, u64 *size_out)
{
    u8 *data = NULL;

    FILE *file = fopen(path, "rb");
    if (!file)
    {
        Log(ERROR, "failed to open file: %s", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size < 0)
    {
        Log(ERROR, "failed to read file: %s", path);
        goto exit;
    }

    data = arena_push_array_no_zero(arena, u8, (u64)size);
    if (fread(data, 1, (u64)size, file) != (u64)size)
    {
        Log(ERROR, "failed to read file: %s", path);
        data = NULL;
    }

    if (size_out)
        *size_out = (u64)size;

exit:
    fclose(file);
    return data;
}
