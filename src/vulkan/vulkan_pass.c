#include "vulkan_pass.h"
#include <vulkan/vulkan_core.h>
#include "core.h"
#include "vulkan_renderer.h"

#define MAX_PIPELINES_PER_PASS 8 // TODO: this need be dynamic

typedef struct _swapchain_target_t swapchain_target_t;
struct _swapchain_target_t
{
    VkSwapchainKHR  swapchain_handle;

    VkImageView     color_imagevies[MAX_FRAMES_IN_FLIGHT];

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


typedef struct
{
    target_type_t type;
    union
    {
        swapchain_target_t  swapchain_target;
        image_target_t      image_target;
    };
} render_target_t;


typedef struct _render_pass_t render_pass_t;
struct _render_pass_t
{
    u64             handle;
    VkExtent2D      extent;
    render_target_t target;
    VkRenderPass    vk_render_pass;
    // pipeline_t pipelines[MAX_PIPELINES_PER_PASS];
    // u64        pipeline_count;
    bool            active;
};

typedef struct _vk_passes_t vk_passes_t;
struct _vk_passes_t
{
    render_pass_t      swapchain_pass;
    bool               swapchain_set;
};

static vk_passes_t g_passes = {};

bool VulkanPass_CreateSwapchainPass(
    VkDevice device, VkPhysicalDeviceMemoryProperties physical_device_memory_properties,
    swapchain_t *swapchain)
{
    Assert(!g_passes.swapchain_set || !g_passes.swapchain_pass.active);

    render_pass_t new_swapchain_pass = {};

    // TODO copy pipelines from existing swapchain if it exists

    // VulkanImage_CreateDepthResources();

    return true;
}
