#ifndef PLATFORM_VULKAN_H
#define PLATFORM_VULKAN_H

#include <vulkan/vulkan_core.h>

#include "platform.h"

/* the returned array is owned by the platform layer */
const char *const *Platform_Vulkan_GetInstanceExtensions(u32 *count);

bool Platform_Vulkan_CreateSurface(platform_window_t *window, VkInstance instance,
                                   VkSurfaceKHR *surface);

#endif
