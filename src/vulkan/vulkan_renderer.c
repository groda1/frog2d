#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "core.h"
#include "log.h"

#include "vulkan_context.h"
#include "vulkan_renderer.h"
#include "memory_arena.h"
#include "vulkan_buffer.h"
#include "vulkan_image.h"
#include "vulkan_pass.h"
#include "vulkan_texture.h"

#define APPLICATION_NAME    "todo"
#define APPLICATION_VERSION VK_MAKE_VERSION(0, 0, 1)
#define ENGINE_NAME         "dcfs"
#define ENGINE_VERSION      VK_MAKE_VERSION(0, 0, 1)

#define MAX_FAMILY_COUNT            16
#define MAX_QUEUE_PRIORITY_COUNT    8
#define MAX_LAYER_COUNT             16
#define MAX_PROPERTY_COUNT          MAX_LAYER_COUNT
#define MAX_SURFACE_FORMATS         8
#define MAX_PRESENT_MODES           8

#define MAX_STATIC_BUFFERS          256

/* backend-global vulkan state, see vulkan_context.h */
VkDevice                         g_device;
VkPhysicalDevice                 g_physical_device;
VkPhysicalDeviceMemoryProperties g_memory_properties;

typedef struct _queue_families_t queue_families_t;
struct _queue_families_t
{
    u32 graphics_family_index;
    u32 transfer_family_index;
    u32 compute_family_index;
    u32 present_family_index;
    u32 graphics_queue_index;
    u32 transfer_queue_index;
    u32 compute_queue_index;
    u32 present_queue_index;

    VkQueue graphics_queue;
    VkQueue transfer_queue;
    VkQueue present_queue;
};

typedef struct _frame_sync_t frame_sync_t;
struct _frame_sync_t
{
    VkSemaphore image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore transfer_finished_semaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore render_finished_semaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence     inflight_fences[MAX_FRAMES_IN_FLIGHT];

    u32 inflight_counter;
};

struct _vk_renderer_t
{
    // memory
    arena_t *global_arena;      // Eternal lifetime
    arena_t *frame_arena;       // Frame lifetime
    arena_t *swapchain_arena;   // Swapchain lifetime

    // data
    VkInstance                          instance;
    VkSurfaceKHR                        surface;
    VkExtent2D                          window_extent;
    VkCommandPool                       command_pool;
    VkCommandBuffer                     draw_command_buffers[MAX_FRAMES_IN_FLIGHT];
    VkCommandBuffer                     transfer_command_buffers[MAX_FRAMES_IN_FLIGHT];

    queue_families_t                    queue_families;
    swapchain_t                         swapchain;
    frame_sync_t                        frame_sync;
};


static vk_renderer_t *s_renderer;

static bool create_swapchain(bool vsync);
static void destroy_swapchain();
static bool recreate_swapchain();
static void recover_failed_frame();
static bool create_sync_objects();
static void destroy_sync_objects();
static bool query_instance_layer_support(string layer_name);
static void log_instance_layer_properties();
static bool create_instance();
static bool create_surface(SDL_Window *window);
static bool setup_physical_device();
static bool create_logical_device();

// Sync
static VkSemaphore  sync_image_available_semaphore();
static VkSemaphore  sync_render_finished_semaphore(u32 image_index);
static VkSemaphore  sync_transfer_finished_semaphore(u32 image_index);
static VkFence      sync_inflight_fence();
static void         sync_step();

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData);


bool VulkanRenderer_Init(arena_t *arena, SDL_Window *window)
{
    u64 pos = MemoryArena_Pos(arena);
    s_renderer = arena_push(arena, vk_renderer_t);

    s_renderer->global_arena =      arena;
    s_renderer->frame_arena =       MemoryArena_CreateP("frame-arena", (arena_params_t){.reserve_size = MB(4), .commit_size = KB(64)});
    s_renderer->swapchain_arena =   MemoryArena_CreateP("swapchain-arena", (arena_params_t){.reserve_size = MB(4), .commit_size = KB(64)});

    log_instance_layer_properties();

    if (!create_instance())
        goto fail;
    if (!create_surface(window))
        goto fail;
    if (!setup_physical_device())
        goto fail;
    if (!create_logical_device())
        goto fail;
    if (!create_swapchain(false))
        goto fail;
    if (!create_sync_objects())
        goto fail;

    if (!VulkanPass_Init(s_renderer->frame_arena))
        goto fail;
    if (!VulkanBuffer_Init())
        goto fail;
    if (!VulkanTexture_Init())
        goto fail;
    if (!VulkanPass_CreateSwapchainPass(s_renderer->global_arena, &s_renderer->swapchain))
        goto fail;


    Log(INFO, "Vulkan renderer initialized");
    return true;

fail:
    MemoryArena_PopTo(arena, pos);

    if (s_renderer->frame_arena)
        MemoryArena_Destroy(s_renderer->frame_arena);
    if (s_renderer->swapchain_arena)
        MemoryArena_Destroy(s_renderer->swapchain_arena);
    s_renderer = NULL;
    return false;
}

bool VulkanRenderer_Destroy()
{
    if (s_renderer)
    {
        VulkanRenderer_WaitIdle();

        destroy_sync_objects();

        VulkanPass_Destroy();

        destroy_swapchain();

        VulkanBuffer_Destroy();
        VulkanTexture_Destroy();

        vkDestroyCommandPool(g_device, s_renderer->command_pool, NULL);
        vkDestroyDevice(g_device, NULL);
        vkDestroySurfaceKHR(s_renderer->instance, s_renderer->surface, NULL);
        vkDestroyInstance(s_renderer->instance, NULL);

        MemoryArena_Print(s_renderer->swapchain_arena);
        MemoryArena_Destroy(s_renderer->swapchain_arena);
        MemoryArena_Print(s_renderer->frame_arena);
        MemoryArena_Destroy(s_renderer->frame_arena);
    }

    return true;
}

bool VulkanRenderer_HandleResize(u32 width, u32 height)
{
    if (width == 0 || height == 0)
        return true; /* minimized; keep the old swapchain */

    s_renderer->window_extent = (VkExtent2D){width, height};

    return recreate_swapchain();
}

void VulkanRenderer_BeginFrame()
{
    MemoryArena_Clear(s_renderer->frame_arena);

    VulkanPass_BeginFrame();
}


bool VulkanRenderer_EndFrame()
{
    VkFence inflight_fence = sync_inflight_fence();
    VkSemaphore signal_semaphore;

    if (vkWaitForFences(g_device, 1, &inflight_fence, VK_TRUE, U64_MAX) != VK_SUCCESS)
    {
        Log(ERROR, "failed to wait for inflight fence");
        return false;
    }

    u32 image_index;
    VkResult result = vkAcquireNextImageKHR(g_device, s_renderer->swapchain.handle,
                                            U64_MAX, sync_image_available_semaphore(),
                                            VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        /* skip this frame; the inflight fence was not reset so the next
           frame's wait passes right through */
        recreate_swapchain();
        return false;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        Log(ERROR, "failed to acquire swapchain image");
        return false;
    }

    VkCommandBuffer transfer_command_buffer =
        s_renderer->transfer_command_buffers[s_renderer->frame_sync.inflight_counter];
    VkCommandBuffer draw_command_buffer =
        s_renderer->draw_command_buffers[s_renderer->frame_sync.inflight_counter];

    bool transfer_required = VulkanBuffer_BakeCommandBuffer(transfer_command_buffer, image_index);

    if (!VulkanPass_BakeCommandBuffer(draw_command_buffer, image_index))
    {
        Log(ERROR, "failed to bake draw command buffer");
        goto error;
    }

    if (transfer_required)
    {
        signal_semaphore = sync_transfer_finished_semaphore(image_index);

        VkSubmitInfo transfer_submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &transfer_command_buffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &signal_semaphore,
        };

        if (vkQueueSubmit(s_renderer->queue_families.transfer_queue, 1, &transfer_submit_info, VK_NULL_HANDLE) !=
            VK_SUCCESS)
        {
            Log(ERROR, "failed to submit transfer command buffer");
            goto error;
        }
    }

    vkResetFences(g_device, 1, &inflight_fence);

    VkSemaphore wait_semaphores[2];
    VkPipelineStageFlags wait_stages[2];
    u8 semaphore_count = 0;

    if (transfer_required)
    {
        wait_semaphores[semaphore_count] = sync_transfer_finished_semaphore(image_index);
        wait_stages[semaphore_count++] = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    }
    wait_semaphores[semaphore_count] = sync_image_available_semaphore();
    wait_stages[semaphore_count++] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    signal_semaphore = sync_render_finished_semaphore(image_index);

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = semaphore_count,
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &draw_command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &signal_semaphore,
    };

    if (vkQueueSubmit(s_renderer->queue_families.graphics_queue, 1, &submit_info,
                      inflight_fence) != VK_SUCCESS)
    {
        Log(ERROR, "failed to submit draw command buffer");
        goto error;
    }

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &signal_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &s_renderer->swapchain.handle,
        .pImageIndices = &image_index,
    };

    result = vkQueuePresentKHR(s_renderer->queue_families.present_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        recreate_swapchain();
    }
    else if (result != VK_SUCCESS)
    {
        Log(ERROR, "failed to present swapchain image");
        goto error;
    }

    sync_step();

    return true;
error:
    recover_failed_frame();
    return false;
}

void VulkanRenderer_WaitIdle()
{
    vkDeviceWaitIdle(g_device);
}

VkExtent2D VulkanRenderer_GetExtent()
{
    return s_renderer->swapchain.extent;
}

pipeline_handle_t VulkanRenderer_AddPipeline(renderpass_handle_t pass_handle,
                                             const pipeline_config_t *config)
{
    return VulkanPass_AddPipeline(pass_handle, config);
}

VkBuffer VulkanRenderer_CreateStaticVertexBuffer(const void *vertices, u64 size)
{
    // TODO remove function
    VkBuffer buffer = VulkanBuffer_CreateStatic(
        s_renderer->command_pool, s_renderer->queue_families.graphics_queue, vertices, size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    return buffer;
}

VkBuffer VulkanRenderer_CreateStaticIndexBuffer(const u32 *indices, u32 index_count)
{
    // TODO remove function
    VkBuffer buffer = VulkanBuffer_CreateStatic(
        s_renderer->command_pool, s_renderer->queue_families.graphics_queue, (const u8 *)indices,
        index_count * sizeof(u32), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    return buffer;
}

buffer_object_handle_t VulkanRenderer_CreateUniformBuffer(u64 size, uniform_stage_t stage)
{
    buffer_object_type_t type =
        stage == UNIFORM_STAGE_VERTEX ? BO_UNIFORM_VERTEX : BO_UNIFORM_FRAGMENT;

    return VulkanBuffer_CreateObject(s_renderer->global_arena, size, type);
}

bool VulkanRenderer_SetBufferObject(buffer_object_handle_t handle, const void *data, u64 size)
{
    return VulkanBuffer_SetObjectData(handle, data, size);
}

texture_handle_t VulkanRenderer_CreateTexture(u32 width, u32 height, const u8 *rgba_data,
                                              sampler_handle_t sampler)
{
    return VulkanTexture_Create(s_renderer->command_pool,
                                s_renderer->queue_families.graphics_queue, width, height,
                                rgba_data, sampler);
}

sampler_handle_t VulkanRenderer_CreateSampler()
{
    return VulkanTexture_CreateSampler();
}

static bool create_swapchain(bool vsync)
{
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR formats[MAX_SURFACE_FORMATS];
    u32 format_count = MAX_SURFACE_FORMATS;
    VkExtent2D extent;

    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physical_device, s_renderer->surface,
            &capabilities) != VK_SUCCESS)
        return false;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(g_physical_device, s_renderer->surface,
            &format_count, formats) != VK_SUCCESS)
        return false;


    /* choose a swapchain surface format */
    VkSurfaceFormatKHR format = {0};
    for (u32 i = 0; i< format_count; i++)
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            format = formats[i];
    if (format.format != VK_FORMAT_B8G8R8A8_SRGB)
    {
        Log(ERROR, "found no suitable swapchain surface format");
        return false;
    }

    /* choose swapchain present mode */
    VkPresentModeKHR present_mode =
        vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;

    /* verify surface capabilities */
    if (capabilities.currentExtent.height == U32_MAX ||
        capabilities.currentExtent.width == U32_MAX) {
        extent.width = Clamp(capabilities.minImageExtent.width,
                             s_renderer->window_extent.width,
                             capabilities.maxImageExtent.width);
        extent.height = Clamp(capabilities.minImageExtent.height,
                              s_renderer->window_extent.height,
                              capabilities.maxImageExtent.height);
    } else {
        extent = capabilities.currentExtent;
    }

    Log(DEBUG, "extent %u, %u", extent.width, extent.height);

    /* verify image count */
    if (capabilities.minImageCount > MAX_FRAMES_IN_FLIGHT ||
        capabilities.maxImageCount < MAX_FRAMES_IN_FLIGHT)
    {
        Log(ERROR, "unsupported swapchain image count: min=%d max=%d", capabilities.minImageCount,
            capabilities.maxImageCount);
        return false;
    }

    u32 queue_families[2] = {s_renderer->queue_families.graphics_family_index, s_renderer->queue_families.present_family_index};
    VkSharingMode image_sharing_mode;
    if (queue_families[0] != queue_families[1])
        image_sharing_mode = VK_SHARING_MODE_CONCURRENT;
    else
        image_sharing_mode = VK_SHARING_MODE_EXCLUSIVE;

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = s_renderer->surface,
        .minImageCount = MAX_FRAMES_IN_FLIGHT,
        .imageColorSpace = format.colorSpace,
        .imageFormat = format.format,
        .imageExtent = extent,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = image_sharing_mode,
        .pQueueFamilyIndices = queue_families,
        .queueFamilyIndexCount = ArrayCount(queue_families),
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = true,
        .imageArrayLayers = 1
    };

    if (vkCreateSwapchainKHR(g_device,
                             &create_info,
                             NULL,
                             &s_renderer->swapchain.handle) != VK_SUCCESS)
    {
        Log(ERROR, "failed to create swapchain");
        return false;
    }

    u32 image_count = MAX_FRAMES_IN_FLIGHT;
    VkResult ret = vkGetSwapchainImagesKHR(g_device, s_renderer->swapchain.handle,
                                           &image_count, s_renderer->swapchain.images);
    if (ret != VK_SUCCESS && ret != VK_INCOMPLETE)
    {
        Log(ERROR, "failed to fetch swapchain images");
        return false;
    }

    Log(INFO, "swapchain image count: %d", image_count);

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (!VulkanImage_CreateView(s_renderer->swapchain.images[i], format.format,
                                    VK_IMAGE_ASPECT_COLOR_BIT, 1,
                                    &s_renderer->swapchain.image_views[i]))
        {
            Log(ERROR, "failed to create swapchain image view");
            return false;
        }
    }

    s_renderer->swapchain.extent = extent;
    s_renderer->swapchain.format = format.format;

    return true;
}

static void destroy_swapchain()
{
    swapchain_t *swapchain = &s_renderer->swapchain;

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        vkDestroyImageView(g_device, swapchain->image_views[i], NULL);

    vkDestroySwapchainKHR(g_device, swapchain->handle, NULL);

    MemoryZeroItem(swapchain);
}

static bool recreate_swapchain()
{
    VulkanRenderer_WaitIdle();

    destroy_swapchain();

    if (!create_swapchain(false))
    {
        Log(ERROR, "failed to recreate swapchain");
        return false;
    }

    if (!VulkanPass_RecreateSwapchainPass(&s_renderer->swapchain))
        return false;

    return true;
}

static void recover_failed_frame()
{
    Log(WARNING, "recovering from failed frame");

    VulkanRenderer_WaitIdle();

    destroy_sync_objects();
    if (!create_sync_objects())
        Log(ERROR, "failed to recreate sync objects during frame recovery");

    if (!recreate_swapchain())
        Log(ERROR, "failed to recreate swapchain during frame recovery");
}

static bool create_sync_objects()
{
    frame_sync_t *sync = &s_renderer->frame_sync;

    VkSemaphoreCreateInfo semaphore_create = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkFenceCreateInfo fence_create = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateSemaphore(g_device, &semaphore_create, NULL,
                              &sync->image_available_semaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(g_device, &semaphore_create, NULL,
                              &sync->transfer_finished_semaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(g_device, &semaphore_create, NULL,
                              &sync->render_finished_semaphores[i]) != VK_SUCCESS ||
            vkCreateFence(g_device, &fence_create, NULL,
                          &sync->inflight_fences[i]) != VK_SUCCESS)
        {
            Log(ERROR, "failed to create frame sync objects");
            return false;
        }
    }

    return true;
}

static void destroy_sync_objects()
{
    frame_sync_t *sync = &s_renderer->frame_sync;

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroySemaphore(g_device, sync->image_available_semaphores[i], NULL);
        vkDestroySemaphore(g_device, sync->transfer_finished_semaphores[i], NULL);
        vkDestroySemaphore(g_device, sync->render_finished_semaphores[i], NULL);
        vkDestroyFence(g_device, sync->inflight_fences[i], NULL);
    }
}

static inline VkSemaphore sync_image_available_semaphore()
{
    return s_renderer->frame_sync.image_available_semaphores[s_renderer->frame_sync.inflight_counter];
}

static inline VkSemaphore sync_transfer_finished_semaphore(u32 image_index)
{
    return s_renderer->frame_sync.transfer_finished_semaphores[image_index];
}

static inline VkSemaphore sync_render_finished_semaphore(u32 image_index)
{
    return s_renderer->frame_sync.render_finished_semaphores[image_index];
}

static inline VkFence sync_inflight_fence()
{
    return s_renderer->frame_sync.inflight_fences[s_renderer->frame_sync.inflight_counter];
}

static void sync_step()
{
    s_renderer->frame_sync.inflight_counter =
        (s_renderer->frame_sync.inflight_counter + 1) % MAX_FRAMES_IN_FLIGHT;
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

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = APPLICATION_NAME,
        .applicationVersion = APPLICATION_VERSION,
        .pEngineName = ENGINE_NAME,
        .engineVersion = ENGINE_VERSION,
        .apiVersion = VK_API_VERSION_1_4,
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


    if (vkCreateInstance(&create_info, NULL, &s_renderer->instance) != VK_SUCCESS)
    {
        Log(ERROR, "failed to create vulkan instance");
        return false;
    }

    Log(INFO, "Created vulkan instance");
    return true;
}

static bool create_surface(SDL_Window *window)
{
    if (!SDL_Vulkan_CreateSurface(window, s_renderer->instance, NULL, &s_renderer->surface))
    {
        Log(ERROR, "Failed to create surface: %s", SDL_GetError());
        return false;
    }

    int width, height;
    if (!(SDL_GetWindowSize(window, &width, &height)))
    {
        Log(ERROR, "failed to get window size");
        return false;
    }
    s_renderer->window_extent = (VkExtent2D){(u32)width, (u32)height};

    Log(INFO, "Created surface");
    return true;
}

static bool setup_physical_device()
{
    VkPhysicalDevice physical_devices[8];
    u32 device_count = 8;

    if (vkEnumeratePhysicalDevices(s_renderer->instance, &device_count, physical_devices) != VK_SUCCESS)
    {
            Log(ERROR, "Failed to enumerate physical devices");
            return false;
    }
    if (device_count == 0)
    {
            Log(ERROR, "No available physical device");
            return false;
    }

    for (u32 i = 0; i < device_count; i++)
    {
        /* TODO:
        Check for queue family support
        Check for extensions:
            - DEVICE_EXTENSIONS
        Check for swap chain support
        Check for anisotropic filtering
        Check for the descriptor indexing features
         */
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physical_devices[i], &properties);

        if (properties.apiVersion < VK_API_VERSION_1_4)
        {
            Log(WARNING, "skipping physical device %s: vulkan %d.%d < 1.4",
                properties.deviceName,
                VK_API_VERSION_MAJOR(properties.apiVersion),
                VK_API_VERSION_MINOR(properties.apiVersion));
            continue;
        }

        g_physical_device = physical_devices[i];

        Log(INFO, "picked physical device: %d %s [vulkan %d.%d.%d]",
            properties.deviceID, properties.deviceName,
            VK_API_VERSION_MAJOR(properties.apiVersion),
            VK_API_VERSION_MINOR(properties.apiVersion),
            VK_API_VERSION_PATCH(properties.apiVersion));
        break;
    }

    if (!g_physical_device)
        return false;

    vkGetPhysicalDeviceMemoryProperties(g_physical_device, &g_memory_properties);

    u32 family_count = MAX_FAMILY_COUNT;
    VkQueueFamilyProperties family_properties[MAX_FAMILY_COUNT];

    queue_families_t queue_families = {
        .graphics_family_index = U32_MAX,
        .transfer_family_index = U32_MAX,
        .compute_family_index = U32_MAX,
        .present_family_index = U32_MAX,
    };

    u32 used_queues_per_family[MAX_FAMILY_COUNT];
    MemoryZeroArray(used_queues_per_family);

    vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &family_count, family_properties);
    Log(DEBUG, "queue family count: %d", family_count);

    for (u32 i = 0 ; i< family_count; i++)
    {
        VkQueueFamilyProperties prop = family_properties[i];
        Log(DEBUG, "queue family %d: queue_count=%d gfx=%d transfer=%d compute=%d",
            i, prop.queueCount,
            prop.queueFlags & VK_QUEUE_GRAPHICS_BIT,
            prop.queueFlags & VK_QUEUE_TRANSFER_BIT,
            prop.queueFlags & VK_QUEUE_COMPUTE_BIT);

        if ((queue_families.graphics_family_index == U32_MAX) && (prop.queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            if (used_queues_per_family[i] < prop.queueCount)
            {
                queue_families.graphics_family_index = i;
                queue_families.graphics_queue_index = used_queues_per_family[i];
                used_queues_per_family[i]++;
            }
        }
        if ((queue_families.transfer_family_index == U32_MAX) && (prop.queueFlags & VK_QUEUE_TRANSFER_BIT))
        {
            if (used_queues_per_family[i] < prop.queueCount)
            {
                queue_families.transfer_family_index = i;
                queue_families.transfer_queue_index = used_queues_per_family[i];
                used_queues_per_family[i]++;
            }
        }
        if ((queue_families.compute_family_index == U32_MAX) && (prop.queueFlags & VK_QUEUE_COMPUTE_BIT))
        {
            if (used_queues_per_family[i] < prop.queueCount)
            {
                queue_families.compute_family_index = i;
                queue_families.compute_queue_index = used_queues_per_family[i];
                used_queues_per_family[i]++;
            }
        }
        if (queue_families.present_family_index == U32_MAX)
        {
            u32 present_supported;

            if (vkGetPhysicalDeviceSurfaceSupportKHR(
                    g_physical_device, i, s_renderer->surface, &present_supported) == VK_SUCCESS &&
                present_supported)
                queue_families.present_family_index = i;
        }
    }

    if (queue_families.graphics_family_index == U32_MAX
            || queue_families.transfer_family_index == U32_MAX
            || queue_families.compute_family_index == U32_MAX
            || queue_families.present_family_index == U32_MAX)
    {
        Log(ERROR, "failed to find all requried devices queues");
        return false;
    }
    Log(DEBUG, "Queues: gfx=%d [%d] transfer=%d [%d] compute=%d [%d] present=%d [%d]",
        queue_families.graphics_family_index,
        queue_families.graphics_queue_index,
        queue_families.transfer_family_index,
        queue_families.transfer_queue_index,
        queue_families.compute_family_index,
        queue_families.compute_queue_index,
        queue_families.present_family_index,
        queue_families.present_queue_index);

    s_renderer->queue_families = queue_families;

    return true;
}

static bool create_logical_device()
{
    u8 queue_count_by_family[MAX_FAMILY_COUNT] = {0};
    queue_families_t *families = &s_renderer->queue_families;

    queue_count_by_family[families->graphics_family_index]++;
    queue_count_by_family[families->transfer_family_index]++;
    queue_count_by_family[families->compute_family_index]++;
    queue_count_by_family[families->present_family_index]++;

    f32 queue_priorities[MAX_QUEUE_PRIORITY_COUNT];

    // TODO: separate priorities?
    for (u32 i = 0; i < MAX_QUEUE_PRIORITY_COUNT; i++)
        queue_priorities[i] = 1.0;

    VkDeviceQueueCreateInfo queue_creates[MAX_FAMILY_COUNT];
    u32 queue_create_count = 0;

    for (u32 i = 0; i < MAX_FAMILY_COUNT; i++)
    {
        if (queue_count_by_family[i] > 0)
        {
            VkDeviceQueueCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = i,
                .pQueuePriorities = queue_priorities,
                .queueCount = queue_count_by_family[i],
            };
            queue_creates[queue_create_count++] = create_info;

            Log(DEBUG, "VkDeviceQueueCreateInfo = {.queueFamilyIndex=%d .queueCount=%d}",
                create_info.queueFamilyIndex, create_info.queueCount);
        }
    }

    const char *extensions[] = {"VK_KHR_swapchain"};
    VkPhysicalDeviceFeatures features = {.samplerAnisotropy = true};

    /* rendering without VkRenderPass/VkFramebuffer objects */
    VkPhysicalDeviceVulkan13Features features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .dynamicRendering = true,
    };

    /* bindless textures: one global runtime-sized descriptor array that
       stays bound while texture slots are written at load time */
    VkPhysicalDeviceVulkan12Features features12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &features13,
        .shaderSampledImageArrayNonUniformIndexing = true,
        .descriptorBindingSampledImageUpdateAfterBind = true,
        .descriptorBindingPartiallyBound = true,
        .runtimeDescriptorArray = true,
    };

    VkDeviceCreateInfo device_create = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features12,
        .pQueueCreateInfos = queue_creates,
        .queueCreateInfoCount = queue_create_count,
        .ppEnabledExtensionNames = extensions,
        .enabledExtensionCount = ArrayCount(extensions),
        .pEnabledFeatures = &features,
    };

    if (vkCreateDevice(g_physical_device, &device_create, NULL, &g_device) != VK_SUCCESS)
    {
        Log(ERROR, "failed to create logical device");
        return false;
    }

    vkGetDeviceQueue(g_device, families->graphics_family_index, families->graphics_queue_index, &families->graphics_queue);
    vkGetDeviceQueue(g_device, families->transfer_family_index, families->transfer_queue_index, &families->transfer_queue);
    vkGetDeviceQueue(g_device, families->present_family_index, families->present_queue_index, &families->present_queue);


    VkCommandPoolCreateInfo create_command_pool = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = families->graphics_family_index
    };

    if (vkCreateCommandPool(g_device, &create_command_pool, NULL, &s_renderer->command_pool) != VK_SUCCESS)
    {
        Log(ERROR, "failed to create graphics command pool");
        return false;
    }

    VkCommandBufferAllocateInfo allocate_command_buffers = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = s_renderer->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
    };

    if (vkAllocateCommandBuffers(g_device, &allocate_command_buffers,
                                 s_renderer->draw_command_buffers) != VK_SUCCESS)
    {
        Log(ERROR, "failed to allocate draw command buffers");
        return false;
    }
    if (vkAllocateCommandBuffers(g_device, &allocate_command_buffers,
                                 s_renderer->transfer_command_buffers) != VK_SUCCESS)
    {
        Log(ERROR, "failed to allocate transfer command buffers");
        return false;
    }

    return true;
}


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
