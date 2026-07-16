#include <vulkan/vulkan_core.h>

#include "core.h"
#include "log.h"
#include "render_types.h"
#include "vulkan_buffer.h"
#include "vulkan_context.h"
#include "vulkan_pass.h"
#include "vulkan_image.h"
#include "vulkan_pipeline.h"
#include "vulkan_renderer.h"
#include "vulkan_texture.h"


#define MAX_PIPELINES_PER_PASS 64 // TODO: this need be dynamic
#define MAX_DRAW_COMMANDS_PER_PASS 1024
#define MAX_IMAGE_PASSES 16


typedef struct _swapchain_target_t swapchain_target_t;
struct _swapchain_target_t
{
    /* borrowed from the swapchain; destroyed with it, not with the target */
    VkImage         color_images[MAX_FRAMES_IN_FLIGHT];
    VkImageView     color_image_views[MAX_FRAMES_IN_FLIGHT];

    VkImage         depth_image;
    VkImageView     depth_image_view;
    VkDeviceMemory  depth_image_memory;
};

typedef struct _image_target_t image_target_t;
struct _image_target_t
{
    /* borrowed from the texture registry; destroyed with the texture */
    VkImage         color_image;
    VkImageView     color_image_view;

    VkImage         depth_image;
    VkImageView     depth_image_view;
    VkDeviceMemory  depth_image_memory;
};

typedef enum {
    SWAPCHAIN_TARGET = 0,
    IMAGE_TARGET,
} target_type_t;


typedef struct _render_target render_target_t;
struct _render_target
{
    target_type_t type;
    union
    {
        swapchain_target_t  swapchain_target;
        image_target_t      image_target;
    };
};


typedef struct _render_pass render_pass_t;
struct _render_pass
{
    renderpass_handle_t handle;
    u32             order;
    VkExtent2D      extent;
    render_target_t target;
    VkFormat        color_format;

    pipeline_t      pipelines[MAX_PIPELINES_PER_PASS];
    u64             pipeline_count;

    draw_command_t  *draw_commands;
    u32             draw_command_capacity;
    u32             draw_command_count;

    bool            active;
};

typedef struct _vk_passes vk_passes_t;
struct _vk_passes
{
    render_pass_t       swapchain_pass;
    bool                swapchain_set;
    VkFormat            depth_format;

    /* image pass handles are 1-based indices into image_passes;
       image_pass_order holds the indices sorted by bake order */
    render_pass_t       image_passes[MAX_IMAGE_PASSES];
    u32                 image_pass_order[MAX_IMAGE_PASSES];
    u32                 image_pass_count;

    arena_t             *frame_arena;
};

static vk_passes_t s_passes = {};

static render_pass_t *get_render_pass(renderpass_handle_t pass_handle);
static const pipeline_t *get_pipeline(const render_pass_t *pass, pipeline_handle_t handle);
static bool create_swapchain_target(swapchain_t *swapchain, swapchain_target_t *target);
static bool bake_command_buffer(render_pass_t *pass, VkCommandBuffer command_buffer, u32 image_index);
static void destroy_render_pass(render_pass_t *pass);
static void destroy_swapchain_target(swapchain_target_t *target);

bool VulkanPass_Init(arena_t *frame_arena)
{
    if (!VulkanImage_FindDepthFormat(&s_passes.depth_format))
    {
        Log(ERROR, "failed to find a supported depth format");
        return false;
    }

    s_passes.frame_arena = frame_arena;

    return true;
}

bool VulkanPass_Destroy()
{
    if (s_passes.swapchain_set && s_passes.swapchain_pass.active)
        destroy_render_pass(&s_passes.swapchain_pass);

    s_passes.swapchain_set = false;

    for (u32 i = 0; i < s_passes.image_pass_count; i++)
    {
        if (s_passes.image_passes[i].active)
            destroy_render_pass(&s_passes.image_passes[i]);
    }
    s_passes.image_pass_count = 0;

    return true;
}

bool VulkanPass_CreateSwapchainPass(arena_t *arena, swapchain_t *swapchain)
{
    Assert(!s_passes.swapchain_set && !s_passes.swapchain_pass.active);

    render_pass_t *pass = &s_passes.swapchain_pass;
    MemoryZeroItem(pass);
    pass->target.type = SWAPCHAIN_TARGET;

    if (!create_swapchain_target(swapchain, &pass->target.swapchain_target))
        return false;

    pass->draw_commands = arena_push_array(arena, draw_command_t, MAX_DRAW_COMMANDS_PER_PASS);
    pass->draw_command_capacity = MAX_DRAW_COMMANDS_PER_PASS;

    pass->handle = SWAPCHAIN_PASS_HANDLE;
    pass->color_format = swapchain->format;
    pass->extent = swapchain->extent;
    pass->active = true;

    s_passes.swapchain_set = true;

    Log(INFO, "Created Swapchain pass");

    return true;
}

renderpass_handle_t VulkanPass_CreateImagePass(arena_t *arena, texture_handle_t target_texture,
                                               u32 order)
{
    if (s_passes.image_pass_count >= MAX_IMAGE_PASSES)
    {
        Log(ERROR, "maximum number of image passes reached");
        return RENDERPASS_HANDLE_INVALID;
    }

    render_pass_t *pass = &s_passes.image_passes[s_passes.image_pass_count];
    MemoryZeroItem(pass);
    pass->target.type = IMAGE_TARGET;

    image_target_t *target = &pass->target.image_target;
    target->color_image = VulkanTexture_GetImage(target_texture);
    target->color_image_view = VulkanTexture_GetImageView(target_texture);

    VkExtent2D extent = VulkanTexture_GetExtent(target_texture);
    if (!VulkanImage_CreateDepthResources(extent, s_passes.depth_format, &target->depth_image,
                                          &target->depth_image_view,
                                          &target->depth_image_memory))
    {
        Log(ERROR, "failed to create depth resources for image pass");
        return RENDERPASS_HANDLE_INVALID;
    }

    pass->draw_commands = arena_push_array(arena, draw_command_t, MAX_DRAW_COMMANDS_PER_PASS);
    pass->draw_command_capacity = MAX_DRAW_COMMANDS_PER_PASS;

    pass->handle = (renderpass_handle_t)(s_passes.image_pass_count + 1); /* 1-based */
    pass->order = order;
    pass->color_format = VulkanTexture_GetFormat(target_texture);
    pass->extent = extent;
    pass->active = true;

    /* keep the bake order sorted; equal orders bake in creation order */
    u32 slot = s_passes.image_pass_count;
    while (slot > 0 &&
           s_passes.image_passes[s_passes.image_pass_order[slot - 1]].order > order)
    {
        s_passes.image_pass_order[slot] = s_passes.image_pass_order[slot - 1];
        slot--;
    }
    s_passes.image_pass_order[slot] = s_passes.image_pass_count;

    s_passes.image_pass_count++;

    Log(INFO, "Created image pass %u [%ux%u] order %u", pass->handle, extent.width,
        extent.height, order);

    return pass->handle;
}

bool VulkanPass_RecreateSwapchainPass(swapchain_t *swapchain)
{
    Assert(s_passes.swapchain_set && s_passes.swapchain_pass.active);

    render_pass_t *pass = &s_passes.swapchain_pass;
    Assert(pass->target.type == SWAPCHAIN_TARGET);

    swapchain_target_t *target = &pass->target.swapchain_target;

    destroy_swapchain_target(target);
    MemoryZeroItem(target);

    if (!create_swapchain_target(swapchain, target))
    {
        pass->active = false;
        return false;
    }

    pass->extent = swapchain->extent;

    Log(INFO, "Recreated swapchain pass [%ux%u]", swapchain->extent.width,
        swapchain->extent.height);

    return true;
}

static bool create_swapchain_target(swapchain_t *swapchain, swapchain_target_t *target)
{
    if (!VulkanImage_CreateDepthResources(swapchain->extent, s_passes.depth_format,
                                          &target->depth_image, &target->depth_image_view,
                                          &target->depth_image_memory))
    {
        Log(ERROR, "failed to create depth resources for swapchain pass");
        return false;
    }

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        target->color_images[i] = swapchain->images[i];
        target->color_image_views[i] = swapchain->image_views[i];
    }

    return true;
}

pipeline_handle_t VulkanPass_AddPipeline(renderpass_handle_t pass_handle,
                                         const pipeline_config_t *config)
{
    render_pass_t *pass = get_render_pass(pass_handle);
    if (!pass)
    {
        Log(ERROR, "no active render pass with handle %u", pass_handle);
        return PIPELINE_HANDLE_INVALID;
    }

    if (pass->pipeline_count >= MAX_PIPELINES_PER_PASS)
    {
        Log(ERROR, "render pass pipeline limit reached");
        return PIPELINE_HANDLE_INVALID;
    }

    pipeline_t *pipeline = &pass->pipelines[pass->pipeline_count];
    if (!VulkanPipeline_Create(pass->color_format, s_passes.depth_format, config, pipeline))
        return PIPELINE_HANDLE_INVALID;

    pass->pipeline_count++;

    return (pipeline_handle_t)pass->pipeline_count; /* 1-based */
}

/* pipeline handles are 1-based indices so 0 stays the invalid handle */
static const pipeline_t *get_pipeline(const render_pass_t *pass, pipeline_handle_t handle)
{
    Assert(handle != PIPELINE_HANDLE_INVALID && handle <= pass->pipeline_count);

    return &pass->pipelines[handle - 1];
}

static render_pass_t *get_render_pass(renderpass_handle_t pass_handle)
{
    if (pass_handle == SWAPCHAIN_PASS_HANDLE && s_passes.swapchain_set &&
        s_passes.swapchain_pass.active)
        return &s_passes.swapchain_pass;

    if (pass_handle != RENDERPASS_HANDLE_INVALID && pass_handle <= s_passes.image_pass_count)
    {
        render_pass_t *pass = &s_passes.image_passes[pass_handle - 1];
        if (pass->active)
            return pass;
    }

    return NULL;
}

void VulkanPass_BeginFrame()
{
    if (s_passes.swapchain_set && s_passes.swapchain_pass.active)
        s_passes.swapchain_pass.draw_command_count = 0;

    for (u32 i = 0; i < s_passes.image_pass_count; i++)
        s_passes.image_passes[i].draw_command_count = 0;
}

void VulkanPass_AddDrawCommand(const draw_command_t *draw_command)
{
    Assert(s_passes.frame_arena != NULL);

    render_pass_t *pass = get_render_pass(draw_command->pass);
    if (!pass)
    {
        Log(ERROR, "no active render pass with handle %u", draw_command->pass);
        return;
    }

    if (pass->draw_command_count >= pass->draw_command_capacity)
    {
        Log(WARNING, "render pass draw command limit reached; command dropped");
        return;
    }

    draw_command_t *slot = &pass->draw_commands[pass->draw_command_count++];
    *slot = *draw_command;

    const pipeline_t *pipeline = get_pipeline(pass, draw_command->pipeline);
    if (pipeline->push_constant_size > 0 && draw_command->push_constant_data)
    {
        u8 *push_constant_copy = arena_push_array_no_zero(s_passes.frame_arena, u8,
                                                          pipeline->push_constant_size);
        MemoryCopy(push_constant_copy, draw_command->push_constant_data,
                   pipeline->push_constant_size);
        slot->push_constant_data = push_constant_copy;
    }
}

bool VulkanPass_BakeCommandBuffer(VkCommandBuffer command_buffer, u32 image_index)
{
    Assert(s_passes.swapchain_set && s_passes.swapchain_pass.active);

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

    /* image passes bake first, in pass order, so their targets are ready to
       be sampled by the passes that follow */
    for (u32 i = 0; i < s_passes.image_pass_count; i++)
    {
        render_pass_t *pass = &s_passes.image_passes[s_passes.image_pass_order[i]];

        if (pass->active && !bake_command_buffer(pass, command_buffer, image_index))
            return false;
    }

    if (!bake_command_buffer(&s_passes.swapchain_pass, command_buffer, image_index))
        return false;

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
    {
        Log(ERROR, "failed to end draw command buffer");
        return false;
    }

    return true;
}

static bool bake_command_buffer(render_pass_t *pass, VkCommandBuffer command_buffer, u32 image_index)
{
    Assert(pass->active);
    Assert(image_index < MAX_FRAMES_IN_FLIGHT);

    bool image_target = pass->target.type == IMAGE_TARGET;

    VkImage color_image = VK_NULL_HANDLE;
    VkImageView color_image_view = VK_NULL_HANDLE;
    VkImage depth_image = VK_NULL_HANDLE;
    VkImageView depth_image_view = VK_NULL_HANDLE;
    switch (pass->target.type)
    {
    case SWAPCHAIN_TARGET:
        color_image = pass->target.swapchain_target.color_images[image_index];
        color_image_view = pass->target.swapchain_target.color_image_views[image_index];
        depth_image = pass->target.swapchain_target.depth_image;
        depth_image_view = pass->target.swapchain_target.depth_image_view;
        break;
    case IMAGE_TARGET:
        color_image = pass->target.image_target.color_image;
        color_image_view = pass->target.image_target.color_image_view;
        depth_image = pass->target.image_target.depth_image;
        depth_image_view = pass->target.image_target.depth_image_view;
        break;
    }

    /* an image target may still be sampled by a previous frame's draws */
    VkImageMemoryBarrier attachment_barriers[] = {
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = image_target ? VK_ACCESS_SHADER_READ_BIT : 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = color_image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = depth_image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        },
    };

    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                             | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                             | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
                             | (image_target ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : 0),
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                             | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                             | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         0, 0, NULL, 0, NULL,
                         ArrayCount(attachment_barriers), attachment_barriers);

    /* image targets clear to transparent black so they composite when drawn
       with alpha blending */
    VkRenderingAttachmentInfo color_attachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = color_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = image_target
            ? (VkClearValue){ .color = { .float32 = {0.0f, 0.0f, 0.0f, 0.0f} } }
            : (VkClearValue){ .color = { .float32 = {0.05f, 0.05f, 0.1f, 1.0f} } },
    };
    VkRenderingAttachmentInfo depth_attachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = depth_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = { .depthStencil = { .depth = 1.0f, .stencil = 0 } },
    };

    VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .offset = {0, 0},
            .extent = pass->extent,
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment,
        .pDepthAttachment = &depth_attachment,
    };

    vkCmdBeginRendering(command_buffer, &rendering_info);

    /* y is flipped to get a gl-style y-up clip space (VK_KHR_maintenance1) */
    VkViewport viewport = {
        .x = 0.0f,
        .y = (f32)pass->extent.height,
        .width = (f32)pass->extent.width,
        .height = -(f32)pass->extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = pass->extent,
    };
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    // TODO sort draw commands by pipeline to minimize rebinds

    const pipeline_t *bound_pipeline = NULL;
    for (u32 i = 0; i < pass->draw_command_count; i++)
    {
        const draw_command_t *command = &pass->draw_commands[i];

        const pipeline_t *pipeline = get_pipeline(pass, command->pipeline);

        if (pipeline != bound_pipeline)
        {
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->vk_pipeline);

            /* set 0 = global bindless textures, set 1 = per-pipeline uniforms */
            VkDescriptorSet descriptor_sets[] = {
                VulkanTexture_GetDescriptorSet(),
                pipeline->descriptor_sets[image_index],
            };
            u32 set_count = pipeline->descriptor_sets[image_index] != VK_NULL_HANDLE ? 2 : 1;

            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipeline->layout, 0, set_count, descriptor_sets, 0, NULL);

            bound_pipeline = pipeline;
        }

        if (pipeline->push_constant_size > 0 && command->push_constant_data)
        {
            vkCmdPushConstants(command_buffer, pipeline->layout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               pipeline->push_constant_size, command->push_constant_data);
        }

        if (command->storage_buffer != BUFFER_OBJECT_HANDLE_INVALID)
        {
            /* the shader dereferences the address via GL_EXT_buffer_reference */
            VkDeviceAddress address =
                VulkanBuffer_GetDeviceAddress(command->storage_buffer, image_index);

            Assert(pipeline->push_constant_size >= sizeof(address));
            vkCmdPushConstants(command_buffer, pipeline->layout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(address), &address);
        }

        VkDeviceSize vertex_buffer_offset = 0;
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &command->vertex_buffer, &vertex_buffer_offset);
        vkCmdBindIndexBuffer(command_buffer, command->index_buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(command_buffer, command->index_count, command->instance_count, 0, 0, 0);
    }

    vkCmdEndRendering(command_buffer);

    /* a swapchain image must be in PRESENT_SRC for vkQueuePresentKHR; an
       image target moves to SHADER_READ_ONLY for sampling by later passes */
    VkImageMemoryBarrier finish_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = image_target ? VK_ACCESS_SHADER_READ_BIT : 0,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = image_target ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                  : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = color_image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         image_target ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
                         0, NULL, 0, NULL, 1, &finish_barrier);

    return true;
}

static void destroy_render_pass(render_pass_t *pass)
{
    Assert(pass->active);

    switch (pass->target.type)
    {
    case SWAPCHAIN_TARGET:
        destroy_swapchain_target(&pass->target.swapchain_target);
        break;
    case IMAGE_TARGET:
        /* the color image is owned by the texture registry */
        vkDestroyImageView(g_device, pass->target.image_target.depth_image_view, NULL);
        vkDestroyImage(g_device, pass->target.image_target.depth_image, NULL);
        vkFreeMemory(g_device, pass->target.image_target.depth_image_memory, NULL);
        break;
    }

    for (u64 i = 0; i < pass->pipeline_count; i++)
        VulkanPipeline_Destroy(&pass->pipelines[i]);
    pass->pipeline_count = 0;

    pass->active = false;
}

static void destroy_swapchain_target(swapchain_target_t *target)
{
    /* depth buffer; the color images/views are owned by the swapchain */
    vkDestroyImageView(g_device, target->depth_image_view, NULL);
    vkDestroyImage(g_device, target->depth_image, NULL);
    vkFreeMemory(g_device, target->depth_image_memory, NULL);
}
