#include <vulkan/vulkan_core.h>

#include "core.h"
#include "log.h"
#include "vulkan_global.h"
#include "vulkan_pass.h"
#include "vulkan_image.h"
#include "vulkan_pipeline.h"
#include "vulkan_renderer.h"


#define MAX_PIPELINES_PER_PASS 64 // TODO: this need be dynamic
#define MAX_DRAW_COMMANDS_PER_PASS 1024


typedef struct _swapchain_target_t swapchain_target_t;
struct _swapchain_target_t
{
    VkSwapchainKHR  swapchain_handle; // TODO: needed?

    VkImageView     color_imageviews[MAX_FRAMES_IN_FLIGHT];

    VkImage         depth_image;
    VkImageView     depth_image_view;
    VkDeviceMemory  depth_image_memory;

    VkFramebuffer   framebuffers[MAX_FRAMES_IN_FLIGHT];
};

typedef struct _image_target_t image_target_t;
struct _image_target_t
{
    u64 todo;
    // TODO;
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
    u64             handle;
    VkExtent2D      extent;
    render_target_t target;
    VkRenderPass    vk_render_pass;

    pipeline_t      pipelines[MAX_PIPELINES_PER_PASS];
    u64             pipeline_count;

    /* draw commands for the current frame */
    draw_command_t  draw_commands[MAX_DRAW_COMMANDS_PER_PASS];
    u32             draw_command_count;

    bool            active;
};

typedef struct _vk_passes vk_passes_t;
struct _vk_passes
{
    render_pass_t       swapchain_pass;
    bool                swapchain_set;
    VkFormat            depth_format;
};

static vk_passes_t g_passes = {};

static render_pass_t *get_render_pass(u64 pass_handle);
static bool create_swapchain_render_pass(VkDevice device, VkFormat color_format,
                                         VkFormat depth_format, VkRenderPass *render_pass_out);
static bool bake_command_buffer(render_pass_t *pass, VkCommandBuffer command_buffer, u32 image_index);
static void destroy_render_pass(VkDevice device, render_pass_t *pass);
static void destroy_swapchain_target(VkDevice device, swapchain_target_t *target);

bool VulkanPass_Init(VkInstance instance, VkPhysicalDevice physical_device)
{
    if (!VulkanImage_FindDepthFormat(instance, physical_device, &g_passes.depth_format))
    {
        Log(ERROR, "failed to find a supported depth format");
        return false;
    }

    return true;
}

bool VulkanPass_Destroy(VkDevice device)
{
    if (g_passes.swapchain_set && g_passes.swapchain_pass.active)
        destroy_render_pass(device, &g_passes.swapchain_pass);

    g_passes.swapchain_set = false;

    return true;
}

bool VulkanPass_CreateSwapchainPass(
    VkDevice device, VkPhysicalDeviceMemoryProperties physical_device_memory_properties,
    swapchain_t *swapchain)
{
    Assert(!g_passes.swapchain_set && !g_passes.swapchain_pass.active);

    render_pass_t pass = {
        .target = {
            .type = SWAPCHAIN_TARGET,
        },
    };

    swapchain_target_t *target = &pass.target.swapchain_target;


    // TODO copy pipelines from existing swapchain pass if it exists

    if (!VulkanImage_CreateDepthResources(device, swapchain->extent,
                                          physical_device_memory_properties,
                                          g_passes.depth_format, &target->depth_image,
                                          &target->depth_image_view, &target->depth_image_memory))
    {
        Log(ERROR, "failed to create depth resources for swapchain pass");
        return false;
    }

    if (!create_swapchain_render_pass(device, swapchain->format, g_passes.depth_format,
                                      &pass.vk_render_pass))
    {
        Log(ERROR, "failed to create swapchain render pass");
        return false;
    }

    if (!VulkanImage_CreateFramebuffers(device, swapchain->image_views, MAX_FRAMES_IN_FLIGHT,
                                        target->depth_image_view, swapchain->extent,
                                        pass.vk_render_pass, target->framebuffers))
    {
        Log(ERROR, "failed to create swapchain framebuffers");
        return false;
    }

    target->swapchain_handle = swapchain->handle;
    MemoryCopyArray(target->color_imageviews, swapchain->image_views);

    pass.handle = SWAPCHAIN_PASS_HANDLE;
    pass.extent = swapchain->extent;
    pass.active = true;

    // TODO rebuild all pipelines

    g_passes.swapchain_pass = pass;
    g_passes.swapchain_set = true;

    Log(INFO, "Created Swapchain pass");

    return true;
}


static bool create_swapchain_render_pass(VkDevice device, VkFormat color_format,
                                         VkFormat depth_format, VkRenderPass *render_pass_out)
{
    // TODO attachments should be optional
    VkAttachmentDescription attachments[] = {
        {
            .format = color_format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        },
        {
            .format = depth_format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        },
    };

    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkAttachmentReference depth_attachment_ref = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
        .pDepthStencilAttachment = &depth_attachment_ref,
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
            | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
            | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
            | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = ArrayCount(attachments),
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    if (vkCreateRenderPass(device, &create_info, NULL /* TODO: Allocator */, render_pass_out) != VK_SUCCESS)
        return false;

    return true;
}

pipeline_handle_t VulkanPass_AddPipeline(arena_t *arena, VkDevice device, renderpass_handle_t pass_handle,
                                         const pipeline_config_t *config)
{
    render_pass_t *pass = get_render_pass(pass_handle);
    if (!pass)
    {
        Log(ERROR, "no active render pass with handle %ju", pass_handle);
        return PIPELINE_HANDLE_INVALID;
    }

    if (pass->pipeline_count >= MAX_PIPELINES_PER_PASS)
    {
        Log(ERROR, "render pass pipeline limit reached");
        return PIPELINE_HANDLE_INVALID;
    }

    // TODO store the config so pipelines can be rebuilt on swapchain recreation

    pipeline_t *pipeline = &pass->pipelines[pass->pipeline_count];
    if (!VulkanPipeline_Create(arena, device, pass->vk_render_pass, pass->extent, config,
                               pipeline))
        return PIPELINE_HANDLE_INVALID;

    return (pipeline_handle_t)pass->pipeline_count++;
}

static render_pass_t *get_render_pass(u64 pass_handle)
{
    if (pass_handle == SWAPCHAIN_PASS_HANDLE && g_passes.swapchain_set &&
        g_passes.swapchain_pass.active)
        return &g_passes.swapchain_pass;

    // TODO image target passes

    return NULL;
}

void VulkanPass_BeginFrame()
{
    if (g_passes.swapchain_set && g_passes.swapchain_pass.active)
        g_passes.swapchain_pass.draw_command_count = 0;

    // TODO image target passes
}

void VulkanPass_AddDrawCommand(const draw_command_t *draw_command)
{
    render_pass_t *pass = get_render_pass(draw_command->pass);
    if (!pass)
    {
        Log(ERROR, "no active render pass with handle %ju", draw_command->pass);
        return;
    }

    if (pass->draw_command_count >= MAX_DRAW_COMMANDS_PER_PASS)
    {
        Log(WARNING, "render pass draw command limit reached; command dropped");
        return;
    }

    pass->draw_commands[pass->draw_command_count++] = *draw_command;
}

bool VulkanPass_BakeCommandBuffer(VkCommandBuffer command_buffer, u32 image_index)
{
    // TODO bake image target passes first, in pass order

    Assert(g_passes.swapchain_set && g_passes.swapchain_pass.active);
    return bake_command_buffer(&g_passes.swapchain_pass, command_buffer, image_index);
}

static bool bake_command_buffer(render_pass_t *pass, VkCommandBuffer command_buffer, u32 image_index)
{
    Assert(pass->active);
    Assert(image_index < MAX_FRAMES_IN_FLIGHT);

    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    switch (pass->target.type)
    {
    case SWAPCHAIN_TARGET:
        framebuffer = pass->target.swapchain_target.framebuffers[image_index];
        break;
    case IMAGE_TARGET:
        // TODO
        return false;
    }

    VkClearValue clear_values[] = {
        { .color = { .float32 = {0.05f, 0.05f, 0.1f, 1.0f} } },
        { .depthStencil = { .depth = 1.0f, .stencil = 0 } },
    };

    VkRenderPassBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = pass->vk_render_pass,
        .framebuffer = framebuffer,
        .renderArea = {
            .offset = {0, 0},
            .extent = pass->extent,
        },
        .clearValueCount = ArrayCount(clear_values),
        .pClearValues = clear_values,
    };

    vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

    // TODO sort draw commands by pipeline to minimize rebinds

    const pipeline_t *bound_pipeline = NULL;
    for (u32 i = 0; i < pass->draw_command_count; i++)
    {
        const draw_command_t *command = &pass->draw_commands[i];

        Assert(command->pipeline < pass->pipeline_count);
        const pipeline_t *pipeline = &pass->pipelines[command->pipeline];

        if (pipeline != bound_pipeline)
        {
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->vk_pipeline);
            bound_pipeline = pipeline;
        }

        if (pipeline->push_constant_size > 0 && command->push_constant_data)
        {
            vkCmdPushConstants(command_buffer, pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                               pipeline->push_constant_size, command->push_constant_data);
        }

        VkDeviceSize vertex_buffer_offset = 0;
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &command->vertex_buffer, &vertex_buffer_offset);
        vkCmdBindIndexBuffer(command_buffer, command->index_buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(command_buffer, command->index_count, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(command_buffer);

    return true;
}

static void destroy_render_pass(VkDevice device, render_pass_t *pass)
{
    Assert(pass->active);

    switch (pass->target.type)
    {
    case SWAPCHAIN_TARGET:
        destroy_swapchain_target(device, &pass->target.swapchain_target);
        break;
    case IMAGE_TARGET:
        // TODO
        break;
    }

    for (u64 i = 0; i < pass->pipeline_count; i++)
        VulkanPipeline_Destroy(device, &pass->pipelines[i]);
    pass->pipeline_count = 0;

    vkDestroyRenderPass(device, pass->vk_render_pass, NULL);

    pass->active = false;
}

static void destroy_swapchain_target(VkDevice device, swapchain_target_t *target)
{
    /* depth buffer */
    vkDestroyImageView(device, target->depth_image_view, NULL);
    vkDestroyImage(device, target->depth_image, NULL);
    vkFreeMemory(device, target->depth_image_memory, NULL);

    /* color buffers */
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        vkDestroyImageView(device, target->color_imageviews[i], NULL);

    /* framebuffers */
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        vkDestroyFramebuffer(device, target->framebuffers[i], NULL);

    vkDestroySwapchainKHR(device, target->swapchain_handle, NULL);
}
