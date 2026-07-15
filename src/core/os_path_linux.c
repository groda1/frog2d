#define _POSIX_C_SOURCE 200809L
#include <unistd.h>

#include "core.h"
#include "os_path.h"

#define MAX_BASE_PATH 1024

static char s_base_path[MAX_BASE_PATH];

const char *OS_GetBasePath(void)
{
    if (s_base_path[0] == '\0')
    {
        ssize_t len = readlink("/proc/self/exe", s_base_path, sizeof(s_base_path) - 1);
        if (len <= 0)
            return "./";
        s_base_path[len] = '\0';

        char *last_slash = strrchr(s_base_path, '/');
        if (last_slash)
            last_slash[1] = '\0';
    }

    return s_base_path;
}
