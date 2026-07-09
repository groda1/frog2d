#include <vulkan/vulkan_core.h>

#include "core.h"
#include "log.h"

#include "vulkan_memory.h"

bool VulkanBuffer_Create(VkDevice device, VkPhysicalDeviceMemoryProperties memory_properties,
                         VkDeviceSize size, VkBufferUsageFlags usage,
                         VkMemoryPropertyFlags memory_flags, VkBuffer *buffer_out,
                         VkDeviceMemory *memory_out)
{
    VkBufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkBuffer buffer;
    if (vkCreateBuffer(device, &create_info, NULL, &buffer) != VK_SUCCESS)
    {
        Log(ERROR, "failed to create buffer (size=%ju)", size);
        return false;
    }

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(device, buffer, &memory_requirements);

    u32 memory_type_index;
    if (!VulkanMemory_FindMemoryType(memory_properties, memory_requirements.memoryTypeBits,
                                     memory_flags, &memory_type_index))
    {
        Log(ERROR, "failed to find a suitable buffer memory type");
        vkDestroyBuffer(device, buffer, NULL);
        return false;
    }

    VkMemoryAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex = memory_type_index,
    };

    VkDeviceMemory memory;
    if (vkAllocateMemory(device, &allocate_info, NULL, &memory) != VK_SUCCESS)
    {
        Log(ERROR, "failed to allocate buffer memory (size=%ju)", memory_requirements.size);
        vkDestroyBuffer(device, buffer, NULL);
        return false;
    }

    if (vkBindBufferMemory(device, buffer, memory, 0) != VK_SUCCESS)
    {
        Log(ERROR, "failed to bind buffer memory");
        vkFreeMemory(device, memory, NULL);
        vkDestroyBuffer(device, buffer, NULL);
        return false;
    }

    *buffer_out = buffer;
    *memory_out = memory;

    return true;
}
