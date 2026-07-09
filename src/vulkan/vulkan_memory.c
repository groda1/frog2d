
#include "vulkan_memory.h"

bool VulkanMemory_FindMemoryType(VkPhysicalDeviceMemoryProperties memory_properties,
                                 u32 type_bits, VkMemoryPropertyFlags required_properties,
                                 u32 *type_index_out)
{
    for (u32 i = 0; i < memory_properties.memoryTypeCount; i++)
    {
        if ((type_bits & (1U << i)) &&
            (memory_properties.memoryTypes[i].propertyFlags & required_properties) == required_properties)
        {
            *type_index_out = i;
            return true;
        }
    }

    return false;
}
