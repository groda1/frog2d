#include <vulkan/vulkan_core.h>

#include "core.h"
#include "log.h"

#include "memory_arena.h"
#include "render_types.h"
#include "vulkan_memory.h"
#include "vulkan_renderer.h"

#define MAX_ASSIGNMENTS     8
#define MAX_BUFFER_OBJECT   1024
#define MAX_STATIC_BUFFERS  256

typedef struct _buffer_object_t buffer_object_t;
struct _buffer_object_t
{
    buffer_object_type_t    type;
    u64                     capacity; // capacity for both the cpu buffer and the memory on the device

    u8                      *cpu_buf;
    u64                     cpu_buf_len;

    VkBuffer                staging_buffers[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory          staging_mem[MAX_FRAMES_IN_FLIGHT];

    VkBuffer                device_buffers[MAX_FRAMES_IN_FLIGHT];

    // TODO: is this only needed for growable BOs?
    //pipeline_handle_t       assigned_pipelines[MAX_ASSIGNMENTS];
    //u32                     assigned_pipelines_count;

    bool dirty[MAX_FRAMES_IN_FLIGHT];
    // TODO growable BOs?
};

static bool copy_buffer_sync(VkDevice device, VkCommandPool command_pool, VkQueue submit_queue,
                             VkBuffer src, VkBuffer dst, VkDeviceSize size);
static bool create_vulkan_buffer(VkDevice device, VkPhysicalDeviceMemoryProperties memory_properties,
                         VkDeviceSize size, VkBufferUsageFlags usage,
                         VkMemoryPropertyFlags memory_flags, VkBuffer *buffer_out,
                         VkDeviceMemory *memory_out);

typedef struct _buffers_t buffers_t;
struct _buffers_t
{
    buffer_object_t *buffer_objects[MAX_BUFFER_OBJECT];
    u32             buffer_object_count;

    VkBuffer        static_buffers[MAX_STATIC_BUFFERS];
    VkDeviceMemory  static_buffer_memories[MAX_STATIC_BUFFERS];
    u32             static_buffer_count;
};

static buffers_t s_buffers = {};


bool VulkanBuffer_Init()
{
    return true;
}

void VulkanBuffer_Destroy(VkDevice device)
{
    // Free static buffers
    for (u32 i = 0; i < s_buffers.static_buffer_count; i++)
    {
        vkDestroyBuffer(device, s_buffers.static_buffers[i], NULL);
        vkFreeMemory(device, s_buffers.static_buffer_memories[i], NULL);
    }

    // Free buffer objects
    // TODO

}


VkBuffer VulkanBuffer_CreateStatic(VkDevice device, VkPhysicalDeviceMemoryProperties memory_prop,
                                   VkCommandPool command_pool, VkQueue submit_queue, const u8 *data,
                                   u64 size, VkBufferUsageFlags usage)
{

    VkBuffer static_buffer = VK_NULL_HANDLE;

    if (s_buffers.static_buffer_count >= MAX_STATIC_BUFFERS)
    {
        Log(ERROR, "maximum number of static buffers reached");
        return VK_NULL_HANDLE;
    }

    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    if (!create_vulkan_buffer(device, memory_prop, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                             &staging_buffer, &staging_memory))
        return VK_NULL_HANDLE;

    void *mapped;
    if (vkMapMemory(device, staging_memory, 0, size, 0, &mapped) != VK_SUCCESS)
    {
        Log(ERROR, "failed to map staging buffer memory");
        goto exit;
    }
    MemoryCopy(mapped, data, size);
    vkUnmapMemory(device, staging_memory);

    VkBuffer buffer;
    VkDeviceMemory memory;
    if (!create_vulkan_buffer(device, memory_prop, size,
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer, &memory))
        goto exit;

    if (!copy_buffer_sync(device, command_pool, submit_queue, staging_buffer, buffer, size))
    {
        vkDestroyBuffer(device, buffer, NULL);
        vkFreeMemory(device, memory, NULL);
        goto exit;
    }

    if (buffer != VK_NULL_HANDLE)
    {
        s_buffers.static_buffers[s_buffers.static_buffer_count] = buffer;
        s_buffers.static_buffer_memories[s_buffers.static_buffer_count] = memory;
        s_buffers.static_buffer_count++;
    }

    static_buffer = buffer;

exit:
    vkDestroyBuffer(device, staging_buffer, NULL);
    vkFreeMemory(device, staging_memory, NULL);


    return static_buffer;
}

AttributeMaybeUnused
buffer_object_handle_t VulkanBuffer_CreateObject(arena_t *arena, VkDevice device, u64 capacity, buffer_object_type_t type)
{
    if (s_buffers.buffer_object_count >= MAX_BUFFER_OBJECT)
        return BUFFER_OBJECT_HANDLE_INVALID;

    buffer_object_handle_t handle = s_buffers.buffer_object_count;
    buffer_object_t *object = arena_push(arena, buffer_object_t);

    *object = (buffer_object_t){
        .capacity = capacity,
        //.device_buffers = todo


    };

    return handle;
}

bool VulkanBuffer_BakeCommandBuffer(VkDevice device, VkCommandBuffer command_buffer, u32 image_index)
{
    bool transfer_required = false;

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    if (vkResetCommandBuffer(command_buffer, 0) != VK_SUCCESS ||
        vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS)
    {
        Log(ERROR, "failed to begin draw command buffer");
        return false;
    }

    for (u32 i = 0 ; i < s_buffers.buffer_object_count; i++)
    {
        buffer_object_t *bo = s_buffers.buffer_objects[i];
        if (!bo->dirty[image_index])
            continue;

        transfer_required = true;

        VkBuffer staging_buffer = bo->staging_buffers[image_index];
        VkDeviceMemory staging_memory = bo->staging_mem[image_index];
        VkBuffer device_buffer = bo->device_buffers[image_index];

        if (bo->cpu_buf_len > 0)
        {
            void *mapped;

            if (vkMapMemory(device, staging_memory, 0, bo->cpu_buf_len, 0, &mapped) != VK_SUCCESS)
            {
                Log(ERROR, "failed to map staging buffer memory");
                return false;
            }
            MemoryCopy(mapped, bo->cpu_buf, bo->cpu_buf_len);
            vkUnmapMemory(device, staging_memory);
        }
        bo->dirty[image_index] = false;

        VkBufferCopy copy_region = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = bo->cpu_buf_len,
        };
        vkCmdCopyBuffer(command_buffer, staging_buffer, device_buffer, 1, &copy_region);
    }

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
    {
        Log(ERROR, "failed to end draw command buffer");
        return false;
    }

    return transfer_required;
}


static bool create_vulkan_buffer(VkDevice device, VkPhysicalDeviceMemoryProperties memory_properties,
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

/* synchronous copy on the graphics queue */
static bool copy_buffer_sync(VkDevice device, VkCommandPool command_pool, VkQueue submit_queue,
                             VkBuffer src, VkBuffer dst, VkDeviceSize size)
{
    VkCommandBufferAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer command_buffer;
    if (vkAllocateCommandBuffers(device, &allocate_info, &command_buffer) != VK_SUCCESS)
    {
        Log(ERROR, "failed to allocate copy command buffer");
        return false;
    }

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    VkBufferCopy region = {
        .size = size,
    };

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
    };

    bool result = false;

    if (vkBeginCommandBuffer(command_buffer, &begin_info) == VK_SUCCESS)
    {
        vkCmdCopyBuffer(command_buffer, src, dst, 1, &region);

        result = vkEndCommandBuffer(command_buffer) == VK_SUCCESS
            && vkQueueSubmit(submit_queue, 1, &submit_info,
                             VK_NULL_HANDLE) == VK_SUCCESS
            && vkQueueWaitIdle(submit_queue) == VK_SUCCESS;
    }

    if (!result)
        Log(ERROR, "failed to record and submit buffer copy");

    vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);

    return result;
}
