#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include "core.h"
#include "log.h"

#include "vulkan_renderer.h"

#define APPLICATION_NAME    "todo"
#define APPLICATION_VERSION VK_MAKE_VERSION(0, 0, 1)
#define ENGINE_NAME         "frog2d"
#define ENGINE_VERSION      VK_MAKE_VERSION(0, 0, 1)

#define MAX_FAMILY_COUNT            16
#define MAX_QUEUE_PRIORITY_COUNT    8
#define MAX_LAYER_COUNT             16
#define MAX_PROPERTY_COUNT          MAX_LAYER_COUNT
#define MAX_SURFACE_FORMATS         8
#define MAX_PRESENT_MODES           8

#define IMAGE_COUNT                 3

typedef struct _queue_families_t queue_families_t;
struct _queue_families_t
{
    u32 graphics_family_index;
    u32 transfer_family_index;
    u32 compute_family_index;
    u32 present_family_index;
    u32 graphics_queue_index;
    u32 transfer_queue_index;
    u32 compute_queue_index;
    u32 present_queue_index;

    VkQueue graphics_queue;
    VkQueue transfer_queue;
    VkQueue present_queue;
};

typedef struct _swapchain_t swapchain_t;
struct _swapchain_t
{
    VkSwapchainKHR swapchain;

    VkImage images[IMAGE_COUNT];
    VkImageView image_views[IMAGE_COUNT];
    VkFormat format;
    VkExtent2D extent;
};

struct _vk_renderer_t
{
    arena_t *arena;
    arena_t *frame_arena;

    /* data */

    VkInstance              instance;
    VkSurfaceKHR            surface;
    VkPhysicalDevice        physical_device;
    VkDevice                device;
    VkCommandPool           command_pool;
    
    queue_families_t        queue_families;
    swapchain_t             swapchain;
};


static vk_renderer_t *renderer;

static bool create_swapchain(bool vsync);
static bool query_instance_layer_support(string layer_name);
static void log_instance_layer_properties();
static bool create_instance();
static bool create_surface(SDL_Window *window);
static bool setup_physical_device();
static bool create_logical_device();


static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
    VkDebugUtilsMessageTypeFlagsEXT messageType, 
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData);

bool VulkanRenderer_Init(arena_t *arena, SDL_Window *window)
{
    u64 pos = MemoryArena_Pos(arena);
    renderer = arena_push(arena, vk_renderer_t);
    
    renderer->arena = arena;

    renderer->frame_arena = MemoryArena_CreateP("frame-arena", (arena_params_t){.reserve_size = MB(4), .commit_size = KB(64)});

    log_instance_layer_properties();

    if (!create_instance())
        goto _fail;
    if (!create_surface(window))
        goto _fail;
    if (!setup_physical_device())
        goto _fail;
    if (!create_logical_device())
        goto _fail;
    if (!create_swapchain(false))
        goto _fail;

    Log(INFO, "Vulkan renderer initialized");
    return true;

_fail:
    MemoryArena_PopTo(arena, pos);
    renderer = NULL;
    return false;    
}

bool VulkanRenderer_Destroy()
{
    if (renderer)
    {
        vkDestroyCommandPool(renderer->device, renderer->command_pool, NULL);
        vkDestroyDevice(renderer->device, NULL);
        vkDestroySurfaceKHR(renderer->instance, renderer->surface, NULL);
        vkDestroyInstance(renderer->instance, NULL);
    }

    MemoryArena_Destroy(renderer->frame_arena);

    return true;
}

static bool create_swapchain(bool vsync)
{
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR formats[MAX_SURFACE_FORMATS];
    u32 format_count = MAX_SURFACE_FORMATS;
    VkExtent2D extent;

    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(renderer->physical_device, renderer->surface, 
            &capabilities) != VK_SUCCESS)
        return false;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->physical_device, renderer->surface, 
            &format_count, formats) != VK_SUCCESS)
        return false;


    Log(DEBUG, "format count %d", format_count);

    /* choose a swapchain surface format */
    VkSurfaceFormatKHR format = {0};
    for (u32 i = 0; i< format_count; i++)
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            format = formats[i];
    if (format.format != VK_FORMAT_B8G8R8A8_SRGB)
    {
        Log(ERROR, "found no suitable swapchain surface format");
        return false;
    }
    
    /* choose swapchain present mode */
    VkPresentModeKHR present_mode = vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;

    /* verify surface capabilities */
    if (capabilities.currentExtent.height == U32_MAX || capabilities.currentExtent.width == U32_MAX)
    {
        Log(ERROR, "bad surface");
        return false;
    }
    extent = capabilities.currentExtent;

    /* verify image count */
    if (capabilities.minImageCount > IMAGE_COUNT || capabilities.maxImageCount < IMAGE_COUNT)
    {
        Log(ERROR, "unsupported swapchain image count: min=%d max=%d", 
                capabilities.minImageCount, capabilities.maxImageCount);
        return false;
    }

    u32 queue_families[2] = {renderer->queue_families.graphics_family_index, renderer->queue_families.present_family_index};
    VkSharingMode image_sharing_mode;
    if (queue_families[0] != queue_families[1])
        image_sharing_mode = VK_SHARING_MODE_CONCURRENT;
    else
        image_sharing_mode = VK_SHARING_MODE_EXCLUSIVE;

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = renderer->surface,
        .minImageCount = IMAGE_COUNT,
        .imageColorSpace = format.colorSpace,
        .imageFormat = format.format,
        .imageExtent = extent,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = image_sharing_mode,
        .pQueueFamilyIndices = queue_families,
        .queueFamilyIndexCount = ArrayCount(queue_families),
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = true,
        .imageArrayLayers = 1
    };

    return false;
}


static bool create_instance()
{
    u32 extension_count;
    const char *extensions[32];
    char const * const * required_extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
    
    for (u32 i = 0; i < extension_count; i++)
        extensions[i] = required_extensions[i];
#ifdef DEBUG_BUILD
    extensions[extension_count++] = "VK_EXT_debug_utils";
#endif
    for (u32 i = 0; i < extension_count; i++)
        Log(DEBUG, "required extension: %s", extensions[i]);

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = APPLICATION_NAME,
        .applicationVersion = APPLICATION_VERSION,
        .pEngineName = ENGINE_NAME,
        .engineVersion = ENGINE_VERSION,
        .apiVersion = VK_API_VERSION_1_0,
    };

    VkInstanceCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.ppEnabledExtensionNames = extensions;
    create_info.enabledExtensionCount = extension_count;

#ifdef DEBUG_BUILD
    VkDebugUtilsMessengerCreateInfoEXT create_info_debug = {0};

    static const char *validation_layer[] = {"VK_LAYER_KHRONOS_validation"};
    if (query_instance_layer_support(string_lit("VK_LAYER_KHRONOS_validation")))
    {
        Log(DEBUG, "Enabling layer: VK_LAYER_KHRONOS_validation.");
        create_info_debug = (VkDebugUtilsMessengerCreateInfoEXT){
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debug_callback,
        };

        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = (const char *const *)&validation_layer;
        create_info.pNext = &create_info_debug;
    }
#endif


    if (vkCreateInstance(&create_info, NULL, &renderer->instance) != VK_SUCCESS)
    {
        Log(ERROR, "failed to create vulkan instance");
        return false;
    }

    Log(INFO, "Created vulkan instance");
    return true;
}

static bool create_surface(SDL_Window *window)
{
    if (!SDL_Vulkan_CreateSurface(window, renderer->instance, NULL, &renderer->surface))
    {
        Log(ERROR, "Failed to create surface: %s", SDL_GetError());
        return false;
    }

    Log(INFO, "Created surface");
    return true;
}

static bool setup_physical_device()
{
    VkPhysicalDevice physical_devices[8];
    u32 device_count = 8;

    if (vkEnumeratePhysicalDevices(renderer->instance, &device_count, physical_devices) != VK_SUCCESS)
    {
            Log(ERROR, "Failed to enumerate physical devices");
            return false;
    }
    if (device_count == 0)
    {
            Log(ERROR, "No available physical device");
            return false;
    }

    for (u32 i = 0; i < device_count; i++)
    {
        /* TODO:
        Check for queue family support
        Check for extensions:
            - DEVICE_EXTENSIONS
        Check for swap chain support
        Check for anisotropic filtering
         */
        if (true)
        {
            VkPhysicalDeviceProperties properties;
            renderer->physical_device = physical_devices[i];
            vkGetPhysicalDeviceProperties(renderer->physical_device, &properties);

            Log(INFO, "picked physical device: %d %s [%d.%d.%d]",
                properties.deviceID,
                properties.deviceName,
                VK_API_VERSION_MAJOR(properties.driverVersion),
                VK_API_VERSION_MINOR(properties.driverVersion),
                VK_API_VERSION_PATCH(properties.driverVersion));
            break;
        }
    }

    if (!renderer->physical_device)
        return false;

    u32 family_count = MAX_FAMILY_COUNT;
    VkQueueFamilyProperties family_properties[MAX_FAMILY_COUNT];


    queue_families_t queue_families = {
        .graphics_family_index = U32_MAX,
        .transfer_family_index = U32_MAX,
        .compute_family_index = U32_MAX,
        .present_family_index = U32_MAX,
    };

    u32 used_queues_per_family[MAX_FAMILY_COUNT];
    MemoryZeroArray(used_queues_per_family);

    vkGetPhysicalDeviceQueueFamilyProperties(renderer->physical_device, &family_count, family_properties);
    Log(DEBUG, "queue family count: %d", family_count);

    for (u32 i = 0 ; i< family_count; i++)
    {
        VkQueueFamilyProperties prop = family_properties[i];
        Log(DEBUG, "queue family %d: queue_count=%d gfx=%d transfer=%d compute=%d",
            i, prop.queueCount,
            prop.queueFlags & VK_QUEUE_GRAPHICS_BIT,
            prop.queueFlags & VK_QUEUE_TRANSFER_BIT,
            prop.queueFlags & VK_QUEUE_COMPUTE_BIT);

        if ((queue_families.graphics_family_index == U32_MAX) && (prop.queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            if (used_queues_per_family[i] < prop.queueCount)
            {
                queue_families.graphics_family_index = i;
                queue_families.graphics_queue_index = used_queues_per_family[i];
                used_queues_per_family[i]++;
            }
        }
        if ((queue_families.transfer_family_index == U32_MAX) && (prop.queueFlags & VK_QUEUE_TRANSFER_BIT))
        {
            if (used_queues_per_family[i] < prop.queueCount)
            {
                queue_families.transfer_family_index = i;
                queue_families.transfer_queue_index = used_queues_per_family[i];
                used_queues_per_family[i]++;
            }
        }
        if ((queue_families.compute_family_index == U32_MAX) && (prop.queueFlags & VK_QUEUE_COMPUTE_BIT))
        {
            if (used_queues_per_family[i] < prop.queueCount)
            {
                queue_families.compute_family_index = i;
                queue_families.compute_queue_index = used_queues_per_family[i];
                used_queues_per_family[i]++;
            }
        }
        if (queue_families.present_family_index == U32_MAX)
        {
            u32 present_supported;

            if (vkGetPhysicalDeviceSurfaceSupportKHR(
                    renderer->physical_device, i, renderer->surface, &present_supported) == VK_SUCCESS &&
                present_supported)
                queue_families.present_family_index = i;
        }
    }

    if (queue_families.graphics_family_index == U32_MAX
            || queue_families.transfer_family_index == U32_MAX
            || queue_families.compute_family_index == U32_MAX
            || queue_families.present_family_index == U32_MAX)
    {
        Log(ERROR, "failed to find all requried devices queues");
        return false;
    }
    Log(DEBUG, "Queues: gfx=%d [%d] transfer=%d [%d] compute=%d [%d] present=%d [%d]",
        queue_families.graphics_family_index,
        queue_families.graphics_queue_index,
        queue_families.transfer_family_index,
        queue_families.transfer_queue_index,
        queue_families.compute_family_index,
        queue_families.compute_queue_index,
        queue_families.present_family_index,
        queue_families.present_queue_index);

    renderer->queue_families = queue_families;

    return true;
}

static bool create_logical_device()
{
    u8 queue_count_by_family[MAX_FAMILY_COUNT] = {0};
    queue_families_t *families = &renderer->queue_families;

    queue_count_by_family[families->graphics_family_index]++;
    queue_count_by_family[families->transfer_family_index]++;
    queue_count_by_family[families->compute_family_index]++;
    queue_count_by_family[families->present_family_index]++;

    f32 queue_priorities[MAX_QUEUE_PRIORITY_COUNT];
    // TODO: separate priorities?
    for (u32 i = 0; i < MAX_QUEUE_PRIORITY_COUNT; i++)
        queue_priorities[i] = 1.0;

    VkDeviceQueueCreateInfo queue_creates[MAX_FAMILY_COUNT];
    u32 queue_create_count = 0;

    for (u32 i = 0; i < MAX_FAMILY_COUNT; i++)
    {
        if (queue_count_by_family[i] > 0)
        {
            VkDeviceQueueCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = i,
                .pQueuePriorities = queue_priorities,
                .queueCount = queue_count_by_family[i],
            };
            queue_creates[queue_create_count++] = create_info;

            Log(DEBUG, "VkDeviceQueueCreateInfo = {.queueFamilyIndex=%d .queueCount=%d}",
                create_info.queueFamilyIndex, create_info.queueCount);
        }
    }

    const char *extensions[] = {"VK_KHR_swapchain", "VK_KHR_maintenance1"};
    VkPhysicalDeviceFeatures features = {.samplerAnisotropy = true};

    VkDeviceCreateInfo device_create = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = queue_creates,
        .queueCreateInfoCount = queue_create_count,
        .ppEnabledExtensionNames = extensions,
        .enabledExtensionCount = ArrayCount(extensions),
        .pEnabledFeatures = &features,
    };

    if (vkCreateDevice(renderer->physical_device, &device_create, NULL, &renderer->device) != VK_SUCCESS)
    {
        Log(ERROR, "failed to create logical device");
        return false;
    }

    vkGetDeviceQueue(renderer->device, families->graphics_family_index, families->graphics_queue_index, &families->graphics_queue);
    vkGetDeviceQueue(renderer->device, families->transfer_family_index, families->transfer_queue_index, &families->transfer_queue);
    vkGetDeviceQueue(renderer->device, families->present_family_index, families->present_queue_index, &families->present_queue);


    VkCommandPoolCreateInfo create_command_pool = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = families->graphics_family_index
    };

    if (vkCreateCommandPool(renderer->device, &create_command_pool, NULL, &renderer->command_pool) != VK_SUCCESS)
    {
        Log(ERROR, "failed to create graphics command pool");
        return false;
    }

    return true;
}

AttributeMaybeUnused
bool query_instance_layer_support(string layer_name)
{
    VkLayerProperties layers[MAX_LAYER_COUNT];
    u32 count = MAX_LAYER_COUNT;

    if (vkEnumerateInstanceLayerProperties(&count, layers) == VK_SUCCESS)
    {
        for (u32 i = 0; i < count; i++)
        {
            if (string_match(layer_name, string_from(layers[i].layerName)))
                return true;
        }
    }
    return false;
}

static void log_instance_layer_properties()
{
    VkLayerProperties layers[MAX_LAYER_COUNT];
    u32 count = MAX_LAYER_COUNT;

    if (vkEnumerateInstanceLayerProperties(&count, layers) == VK_SUCCESS)
    {
        if (count == 0)
            Log(WARNING, "No available instance layers");
        else
            Log(DEBUG, "Available instance layers:");

        for (u32 i = 0; i < count; i++)
        {
            VkLayerProperties *layer = &layers[i];
            VkExtensionProperties properties[MAX_PROPERTY_COUNT];
            u32 property_count = MAX_PROPERTY_COUNT;

            Log(DEBUG, " - %s [%d.%d.%d] - %s",
                layer->layerName,
                VK_API_VERSION_MAJOR(layer->specVersion),
                VK_API_VERSION_MINOR(layer->specVersion),
                VK_API_VERSION_PATCH(layer->specVersion),
                layer->description);

            if (vkEnumerateInstanceExtensionProperties(layer->layerName,
                                                       &property_count,
                                                       properties) == VK_SUCCESS)
            {
                for (u32 j = 0; j < property_count; j++)
                {
                    VkExtensionProperties *property = &properties[j];

                    Log(DEBUG, "       %s [%d.%d.%d]",
                        property->extensionName,
                        VK_API_VERSION_MAJOR(property->specVersion),
                        VK_API_VERSION_MINOR(property->specVersion),
                        VK_API_VERSION_PATCH(property->specVersion));
                }
            }
        }
    }
    else
    {
        Log(ERROR, "Failed to enumerate Instance Layer Properties");
    }
}

AttributeMaybeUnused
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
    VkDebugUtilsMessageTypeFlagsEXT messageType, 
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData)
{
    (void)pUserData;
    const char *type;
    switch (messageType)
    {
    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
        type = "[General]";
        break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
        type = "[Validation]";
        break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
        type = "[Performance]";
        break;
    default:
        type = "[Unknown]";
    }

    switch (messageSeverity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        Log(DEBUG, "%s %s", type, pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        Log(WARNING, "%s %s", type, pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        Log(ERROR, "%s %s", type, pCallbackData->pMessage);
        break;
    default:
    }
    return VK_FALSE;
}
