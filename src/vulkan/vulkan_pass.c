#include <vulkan/vulkan_core.h>

#include "core.h"
#include "log.h"
#include "vulkan_pass.h"
#include "vulkan_image.h"
#include "vulkan_renderer.h"

#define MAX_PIPELINES_PER_PASS 8 // TODO: this need be dynamic

#define SWAPCHAIN_PASS_HANDLE U64_MAX

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

    //pipeline_t      pipelines[MAX_PIPELINES_PER_PASS];
    u64             pipeline_count;

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

static bool create_swapchain_render_pass(VkDevice device, VkFormat color_format,
                                         VkFormat depth_format, VkRenderPass *render_pass_out);
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

static bool bake_command_buffer()

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

    // TODO destroy pipelines

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
