#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "core.h"
#include "os_path.h"

#define MAX_BASE_PATH 1024

static char s_base_path[MAX_BASE_PATH];

const char *OS_GetBasePath(void)
{
    if (s_base_path[0] == '\0')
    {
        WCHAR path_w[MAX_PATH];
        DWORD len = GetModuleFileNameW(NULL, path_w, MAX_PATH);
        if (len == 0 || len == MAX_PATH)
            return ".\\";

        if (!WideCharToMultiByte(CP_UTF8, 0, path_w, -1,
                                 s_base_path, sizeof(s_base_path), NULL, NULL))
            return ".\\";

        char *last_slash = strrchr(s_base_path, '\\');
        if (last_slash)
            last_slash[1] = '\0';
    }

    return s_base_path;
}
