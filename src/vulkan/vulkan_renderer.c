#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include "core.h"
#include "log.h"

#include "vulkan_renderer.h"


#define APPLICATION_NAME    "todo"
#define APPLICATION_VERSION VK_MAKE_VERSION(0, 0, 1)
#define ENGINE_NAME         "frog2d"
#define ENGINE_VERSION      VK_MAKE_VERSION(0, 0, 1)

struct _vk_renderer_t
{
    arena_t *arena;

    /* data */
    VkInstance instance;
};

static vk_renderer_t *renderer;

static bool query_instance_layer_support(string layer_name);
static void log_instance_layer_properties();
static bool create_instance();

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
    VkDebugUtilsMessageTypeFlagsEXT messageType, 
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData);

bool VulkanRenderer_Init(arena_t *arena)
{
    renderer = arena_push(arena, vk_renderer_t);
    renderer->arena = arena;

    log_instance_layer_properties();

    if (!create_instance())
        return false;
        
    return true;
}

bool VulkanRenderer_Destroy()
{
    if (renderer)
        vkDestroyInstance(renderer->instance, NULL);

    return true;
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

    VkApplicationInfo app_info = (VkApplicationInfo){
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
    return true;
}

#define MAX_LAYER_COUNT 16
#define MAX_PROPERTY_COUNT MAX_LAYER_COUNT

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
