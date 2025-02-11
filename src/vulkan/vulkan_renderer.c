#include <vulkan/vulkan.h>

#include "core.h"
#include "log.h"

#include "vulkan_renderer.h"

static vk_renderer_t *renderer;

static void log_instance_layer_properties();

bool VulkanRenderer_Init(arena_t *arena)
{
    renderer = arena_push(arena, vk_renderer_t);
    renderer->arena = arena;

    log_instance_layer_properties();

    return true;
}

static void log_instance_layer_properties()
{
#define MAX_LAYER_COUNT 64
#define MAX_PROPERTY_COUNT MAX_LAYER_COUNT
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
