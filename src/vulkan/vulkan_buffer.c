#include <vulkan/vulkan_core.h>

#include "core.h"
#include "log.h"

#include "memory_arena.h"
#include "render_types.h"
#include "vulkan_buffer.h"
#include "vulkan_context.h"
#include "vulkan_memory.h"
#include "vulkan_types.h"

#define MAX_BUFFER_OBJECT   1024
#define MAX_STATIC_BUFFERS  256
#define MAX_RETIRED_BUFFERS 64

typedef struct _buffer_object_t buffer_object_t;
struct _buffer_object_t
{
    buffer_object_type_t    type;
    u64                     capacity; /* capacity for both the cpu buffer and the memory on the device */

    arena_t                 *arena;
    u8                      *cpu_buf;
    u64                     cpu_buf_len;


    VkBuffer                staging_buffers[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory          staging_mem[MAX_FRAMES_IN_FLIGHT];

    VkBuffer                device_buffers[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory          device_mem[MAX_FRAMES_IN_FLIGHT];

    /* BO_STORAGE only; shaders reach the buffer through this address */
    VkDeviceAddress         device_addresses[MAX_FRAMES_IN_FLIGHT];

    /* size of each frame in flight's buffers; lags behind capacity after a
       grow until the frame is baked and the buffers are rebuilt */
    u64                     buffer_capacities[MAX_FRAMES_IN_FLIGHT];

    bool dirty[MAX_FRAMES_IN_FLIGHT];
};

/* buffers whose last GPU use may still be in flight; destroyed once
   MAX_FRAMES_IN_FLIGHT later frames have waited their fences */
typedef struct _retired_buffer_t retired_buffer_t;
struct _retired_buffer_t
{
    VkBuffer        buffer;
    VkDeviceMemory  memory;
    u64             frame;
};

static bool copy_buffer_sync(VkCommandPool command_pool, VkQueue submit_queue, VkBuffer src,
                             VkBuffer dst, VkDeviceSize size);
static buffer_object_t *get_buffer_object(buffer_object_handle_t handle);
static bool create_vulkan_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                 VkMemoryPropertyFlags memory_flags, VkBuffer *buffer_out,
                                 VkDeviceMemory *memory_out);
static bool create_object_buffers(buffer_object_t *object, u32 frame_index);
static bool grow_object(buffer_object_t *object, u64 required);
static void retire_buffer(VkBuffer buffer, VkDeviceMemory memory);
static void flush_retired_buffers(bool destroy_all);

typedef struct _buffers_t buffers_t;
struct _buffers_t
{
    buffer_object_t *buffer_objects[MAX_BUFFER_OBJECT];
    u32             buffer_object_count;

    VkBuffer        static_buffers[MAX_STATIC_BUFFERS];
    VkDeviceMemory  static_buffer_memories[MAX_STATIC_BUFFERS];
    u32             static_buffer_count;

    retired_buffer_t retired[MAX_RETIRED_BUFFERS];
    u32             retired_count;
    u64             frame_counter; /* one tick per baked frame */
};

static buffers_t s_buffers = {};


/* buffer object handles are 1-based indices so 0 stays the invalid handle */
static buffer_object_t *get_buffer_object(buffer_object_handle_t handle)
{
    Assert(handle != BUFFER_OBJECT_HANDLE_INVALID && handle <= s_buffers.buffer_object_count);

    return s_buffers.buffer_objects[handle - 1];
}

bool VulkanBuffer_Init()
{
    return true;
}

void VulkanBuffer_Destroy()
{
    /* only called after VulkanRenderer_WaitIdle */
    flush_retired_buffers(true);

    // Free static buffers
    for (u32 i = 0; i < s_buffers.static_buffer_count; i++)
    {
        vkDestroyBuffer(g_device, s_buffers.static_buffers[i], NULL);
        vkFreeMemory(g_device, s_buffers.static_buffer_memories[i], NULL);
    }

    // Free buffer objects
    for (u32 i = 0; i < s_buffers.buffer_object_count; i++)
    {
        buffer_object_t *object = s_buffers.buffer_objects[i];

        for (u32 j = 0; j < MAX_FRAMES_IN_FLIGHT; j++)
        {
            vkDestroyBuffer(g_device, object->staging_buffers[j], NULL);
            vkFreeMemory(g_device, object->staging_mem[j], NULL);
            vkDestroyBuffer(g_device, object->device_buffers[j], NULL);
            vkFreeMemory(g_device, object->device_mem[j], NULL);
        }
    }
}


VkBuffer VulkanBuffer_CreateStatic(VkCommandPool command_pool, VkQueue submit_queue,
                                   const u8 *data, u64 size, VkBufferUsageFlags usage)
{

    VkBuffer static_buffer = VK_NULL_HANDLE;

    if (s_buffers.static_buffer_count >= MAX_STATIC_BUFFERS)
    {
        Log(ERROR, "maximum number of static buffers reached");
        return VK_NULL_HANDLE;
    }

    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    if (!VulkanBuffer_CreateStaging(data, size, &staging_buffer, &staging_memory))
        return VK_NULL_HANDLE;

    VkBuffer buffer;
    VkDeviceMemory memory;
    if (!create_vulkan_buffer(size,
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer, &memory))
        goto exit;

    if (!copy_buffer_sync(command_pool, submit_queue, staging_buffer, buffer, size))
    {
        vkDestroyBuffer(g_device, buffer, NULL);
        vkFreeMemory(g_device, memory, NULL);
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
    vkDestroyBuffer(g_device, staging_buffer, NULL);
    vkFreeMemory(g_device, staging_memory, NULL);


    return static_buffer;
}

bool VulkanBuffer_CreateStaging(const void *data, u64 size, VkBuffer *buffer_out,
                                VkDeviceMemory *memory_out)
{
    VkBuffer buffer;
    VkDeviceMemory memory;
    if (!create_vulkan_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              &buffer, &memory))
        return false;

    void *mapped;
    if (vkMapMemory(g_device, memory, 0, size, 0, &mapped) != VK_SUCCESS)
    {
        Log(ERROR, "failed to map staging buffer memory");
        vkDestroyBuffer(g_device, buffer, NULL);
        vkFreeMemory(g_device, memory, NULL);
        return false;
    }
    MemoryCopy(mapped, data, size);
    vkUnmapMemory(g_device, memory);

    *buffer_out = buffer;
    *memory_out = memory;

    return true;
}

buffer_object_handle_t VulkanBuffer_CreateObject(arena_t *arena, u64 capacity,
                                                 buffer_object_type_t type)
{
    if (s_buffers.buffer_object_count >= MAX_BUFFER_OBJECT)
    {
        Log(ERROR, "maximum number of buffer objects reached");
        return BUFFER_OBJECT_HANDLE_INVALID;
    }

    buffer_object_t *object = arena_push(arena, buffer_object_t);
    object->type = type;
    object->capacity = capacity;
    object->cpu_buf = arena_push_array_no_zero(arena, u8, capacity);
    object->arena = arena;

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (!create_object_buffers(object, i))
            return BUFFER_OBJECT_HANDLE_INVALID;
    }

    s_buffers.buffer_objects[s_buffers.buffer_object_count++] = object;

    return (buffer_object_handle_t)s_buffers.buffer_object_count; /* 1-based */
}

bool VulkanBuffer_SetObjectData(buffer_object_handle_t handle, const void *data, u64 size)
{
    if (handle == BUFFER_OBJECT_HANDLE_INVALID || handle > s_buffers.buffer_object_count)
    {
        Log(ERROR, "invalid buffer object handle %u", handle);
        return false;
    }

    buffer_object_t *object = get_buffer_object(handle);
    if (size > object->capacity && !grow_object(object, size))
    {
        Log(ERROR, "buffer object data exceeds capacity (%ju > %ju)", size, object->capacity);
        return false;
    }

    MemoryCopy(object->cpu_buf, data, size);
    object->cpu_buf_len = size;

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        object->dirty[i] = true;

    return true;
}

bool VulkanBuffer_ClearObjectData(buffer_object_handle_t handle)
{
    if (handle == BUFFER_OBJECT_HANDLE_INVALID || handle > s_buffers.buffer_object_count)
    {
        Log(ERROR, "invalid buffer object handle %u", handle);
        return false;
    }

    buffer_object_t *object = get_buffer_object(handle);

    object->cpu_buf_len = 0;

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        object->dirty[i] = true;

    return true;
}

bool VulkanBuffer_PushObjectData(buffer_object_handle_t handle, const void *data, u64 size)
{
    if (handle == BUFFER_OBJECT_HANDLE_INVALID || handle > s_buffers.buffer_object_count)
    {
        Log(ERROR, "invalid buffer object handle %u", handle);
        return false;
    }

    buffer_object_t *object = get_buffer_object(handle);
    if (object->cpu_buf_len + size > object->capacity &&
        !grow_object(object, object->cpu_buf_len + size))
    {
        Log(ERROR, "buffer object data exceeds capacity (%ju > %ju)", object->cpu_buf_len + size, object->capacity);
        return false;
    }

    MemoryCopy(object->cpu_buf + object->cpu_buf_len, data, size);
    object->cpu_buf_len += size;

    return true;
}

VkBuffer VulkanBuffer_GetDeviceBuffer(buffer_object_handle_t handle, u32 frame_index)
{
    Assert(frame_index < MAX_FRAMES_IN_FLIGHT);

    return get_buffer_object(handle)->device_buffers[frame_index];
}

u64 VulkanBuffer_GetObjectCapacity(buffer_object_handle_t handle)
{
    return get_buffer_object(handle)->capacity;
}

VkDeviceAddress VulkanBuffer_GetDeviceAddress(buffer_object_handle_t handle, u32 frame_index)
{
    buffer_object_t *object = get_buffer_object(handle);

    Assert(frame_index < MAX_FRAMES_IN_FLIGHT);
    Assert(object->type == BO_STORAGE);

    return object->device_addresses[frame_index];
}

bool VulkanBuffer_BakeCommandBuffer(VkCommandBuffer command_buffer, u32 image_index)
{
    bool transfer_required = false;

    s_buffers.frame_counter++;
    flush_retired_buffers(false);

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

    for (u32 i = 0; i < s_buffers.buffer_object_count; i++)
    {
        buffer_object_t *bo = s_buffers.buffer_objects[i];
        if (!bo->dirty[image_index])
            continue;

        bo->dirty[image_index] = false;

        /* zero-size copies are not allowed */
        if (bo->cpu_buf_len == 0)
            continue;

        /* the object grew since this frame's buffers were created; rebuild
           them (in-flight frames keep the retired ones) */
        if (Unlikely(bo->buffer_capacities[image_index] < bo->capacity))
        {
            retire_buffer(bo->staging_buffers[image_index], bo->staging_mem[image_index]);
            retire_buffer(bo->device_buffers[image_index], bo->device_mem[image_index]);

            if (!create_object_buffers(bo, image_index))
            {
                Log(ERROR, "failed to grow buffer object buffers");
                return false;
            }

            Log(DEBUG, "buffer object %u grown to %ju bytes", i + 1, bo->capacity);
        }

        VkBuffer staging_buffer = bo->staging_buffers[image_index];
        VkDeviceMemory staging_memory = bo->staging_mem[image_index];
        VkBuffer device_buffer = bo->device_buffers[image_index];

        void *mapped;
        if (vkMapMemory(g_device, staging_memory, 0, bo->cpu_buf_len, 0, &mapped) != VK_SUCCESS)
        {
            Log(ERROR, "failed to map staging buffer memory");
            return false;
        }
        MemoryCopy(mapped, bo->cpu_buf, bo->cpu_buf_len);
        vkUnmapMemory(g_device, staging_memory);

        VkBufferCopy copy_region = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = bo->cpu_buf_len,
        };
        vkCmdCopyBuffer(command_buffer, staging_buffer, device_buffer, 1, &copy_region);

        transfer_required = true;
    }

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
    {
        Log(ERROR, "failed to end draw command buffer");
        return false;
    }

    return transfer_required;
}


/* creates one frame in flight's staging + device buffers at the object's
   current capacity */
static bool create_object_buffers(buffer_object_t *object, u32 frame_index)
{
    u64 capacity = object->capacity;
    VkBufferUsageFlags usage = object->type == BO_STORAGE
        ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
        : VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    if (!create_vulkan_buffer(capacity, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                  | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              &object->staging_buffers[frame_index],
                              &object->staging_mem[frame_index]))
        return false;

    if (!create_vulkan_buffer(capacity,
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                              &object->device_buffers[frame_index],
                              &object->device_mem[frame_index]))
        return false;

    if (object->type == BO_STORAGE)
    {
        VkBufferDeviceAddressInfo address_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = object->device_buffers[frame_index],
        };
        object->device_addresses[frame_index] = vkGetBufferDeviceAddress(g_device, &address_info);
    }

    object->buffer_capacities[frame_index] = capacity;

    return true;
}

/* doubles the cpu buffer until required fits; the device buffers are
   rebuilt lazily at bake time. BO_STORAGE only: uniform buffers are
   referenced by descriptor sets written at pipeline creation, so their
   buffers cannot be swapped out */
static bool grow_object(buffer_object_t *object, u64 required)
{
    if (object->type != BO_STORAGE)
        return false;

    u64 capacity = object->capacity;
    while (capacity < required)
        capacity *= 2;

    /* the old cpu_buf is abandoned in the arena; the waste is bounded by
       the doubling */
    u8 *cpu_buf = arena_push_array_no_zero(object->arena, u8, capacity);
    if (!cpu_buf)
        return false;

    MemoryCopy(cpu_buf, object->cpu_buf, object->cpu_buf_len);
    object->cpu_buf = cpu_buf;
    object->capacity = capacity;

    return true;
}

static void retire_buffer(VkBuffer buffer, VkDeviceMemory memory)
{
    if (s_buffers.retired_count >= MAX_RETIRED_BUFFERS)
    {
        Log(WARNING, "retired buffer list full; forcing device idle");
        vkDeviceWaitIdle(g_device);
        flush_retired_buffers(true);
    }

    retired_buffer_t *slot = &s_buffers.retired[s_buffers.retired_count++];
    slot->buffer = buffer;
    slot->memory = memory;
    slot->frame = s_buffers.frame_counter;
}

static void flush_retired_buffers(bool destroy_all)
{
    u32 kept = 0;

    for (u32 i = 0; i < s_buffers.retired_count; i++)
    {
        retired_buffer_t *retired = &s_buffers.retired[i];

        if (destroy_all ||
            s_buffers.frame_counter - retired->frame >= MAX_FRAMES_IN_FLIGHT)
        {
            vkDestroyBuffer(g_device, retired->buffer, NULL);
            vkFreeMemory(g_device, retired->memory, NULL);
        }
        else
        {
            s_buffers.retired[kept++] = *retired;
        }
    }

    s_buffers.retired_count = kept;
}

static bool create_vulkan_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
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
    if (vkCreateBuffer(g_device, &create_info, NULL, &buffer) != VK_SUCCESS)
    {
        Log(ERROR, "failed to create buffer (size=%ju)", size);
        return false;
    }

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(g_device, buffer, &memory_requirements);

    u32 memory_type_index;
    if (!VulkanMemory_FindMemoryType(g_memory_properties, memory_requirements.memoryTypeBits,
                                     memory_flags, &memory_type_index))
    {
        Log(ERROR, "failed to find a suitable buffer memory type");
        vkDestroyBuffer(g_device, buffer, NULL);
        return false;
    }

    VkMemoryAllocateFlagsInfo allocate_flags = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
    };

    VkMemoryAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT ? &allocate_flags : NULL,
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex = memory_type_index,
    };

    VkDeviceMemory memory;
    if (vkAllocateMemory(g_device, &allocate_info, NULL, &memory) != VK_SUCCESS)
    {
        Log(ERROR, "failed to allocate buffer memory (size=%ju)", memory_requirements.size);
        vkDestroyBuffer(g_device, buffer, NULL);
        return false;
    }

    if (vkBindBufferMemory(g_device, buffer, memory, 0) != VK_SUCCESS)
    {
        Log(ERROR, "failed to bind buffer memory");
        vkFreeMemory(g_device, memory, NULL);
        vkDestroyBuffer(g_device, buffer, NULL);
        return false;
    }

    *buffer_out = buffer;
    *memory_out = memory;

    return true;
}

/* synchronous copy on the graphics queue */
static bool copy_buffer_sync(VkCommandPool command_pool, VkQueue submit_queue, VkBuffer src,
                             VkBuffer dst, VkDeviceSize size)
{
    VkCommandBufferAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer command_buffer;
    if (vkAllocateCommandBuffers(g_device, &allocate_info, &command_buffer) != VK_SUCCESS)
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

    vkFreeCommandBuffers(g_device, command_pool, 1, &command_buffer);

    return result;
}
