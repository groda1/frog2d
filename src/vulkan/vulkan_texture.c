#include <vulkan/vulkan_core.h>

#include "core.h"
#include "log.h"

#include "vulkan_context.h"
#include "vulkan_image.h"
#include "vulkan_texture.h"

#define MAX_TEXTURES 1024
#define MAX_SAMPLERS 16

#define BINDLESS_TEXTURE_BINDING 0

typedef struct _texture_t texture_t;
struct _texture_t
{
    VkImage         image;
    VkDeviceMemory  image_memory;
    VkImageView     image_view;

    u32             width;
    u32             height;
    VkFormat        format;
};

typedef struct _textures_t textures_t;
struct _textures_t
{
    texture_t   textures[MAX_TEXTURES];
    u32         texture_count;

    VkSampler   samplers[MAX_SAMPLERS];
    u32         sampler_count;

    /* one global runtime-sized descriptor array; a texture's handle is its
       element index. handles are 1-based (0 = invalid) so element 0 is
       never written, which the partially-bound binding allows */
    VkDescriptorSetLayout   descriptor_set_layout;
    VkDescriptorPool        descriptor_pool;
    VkDescriptorSet         descriptor_set;
};

static textures_t s_textures = {};


bool VulkanTexture_Init()
{
    VkDescriptorBindingFlags binding_flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
        | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = 1,
        .pBindingFlags = &binding_flags,
    };

    VkDescriptorSetLayoutBinding layout_binding = {
        .binding = BINDLESS_TEXTURE_BINDING,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = MAX_TEXTURES,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };

    VkDescriptorSetLayoutCreateInfo layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &binding_flags_create_info,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = 1,
        .pBindings = &layout_binding,
    };

    if (vkCreateDescriptorSetLayout(g_device, &layout_create_info, NULL,
                                    &s_textures.descriptor_set_layout) != VK_SUCCESS)
    {
        Log(ERROR, "failed to create bindless texture descriptor set layout");
        return false;
    }

    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = MAX_TEXTURES,
    };

    VkDescriptorPoolCreateInfo pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };

    if (vkCreateDescriptorPool(g_device, &pool_create_info, NULL,
                               &s_textures.descriptor_pool) != VK_SUCCESS)
    {
        Log(ERROR, "failed to create bindless texture descriptor pool");
        return false;
    }

    VkDescriptorSetAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = s_textures.descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &s_textures.descriptor_set_layout,
    };

    if (vkAllocateDescriptorSets(g_device, &allocate_info,
                                 &s_textures.descriptor_set) != VK_SUCCESS)
    {
        Log(ERROR, "failed to allocate bindless texture descriptor set");
        return false;
    }

    Log(INFO, "Created bindless texture descriptor set [%u slots]", MAX_TEXTURES);

    return true;
}

void VulkanTexture_Destroy()
{
    for (u32 i = 0; i < s_textures.texture_count; i++)
    {
        texture_t *texture = &s_textures.textures[i];

        vkDestroyImageView(g_device, texture->image_view, NULL);
        vkDestroyImage(g_device, texture->image, NULL);
        vkFreeMemory(g_device, texture->image_memory, NULL);
    }

    for (u32 i = 0; i < s_textures.sampler_count; i++)
        vkDestroySampler(g_device, s_textures.samplers[i], NULL);

    if (s_textures.descriptor_pool != VK_NULL_HANDLE)
        vkDestroyDescriptorPool(g_device, s_textures.descriptor_pool, NULL);
    if (s_textures.descriptor_set_layout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(g_device, s_textures.descriptor_set_layout, NULL);

    MemoryZeroItem(&s_textures);
}

texture_handle_t VulkanTexture_Create(u32 width, u32 height, const u8 *rgba_data,
                                      sampler_handle_t sampler)
{
    if (s_textures.texture_count >= MAX_TEXTURES)
    {
        Log(ERROR, "maximum number of textures reached");
        return TEXTURE_HANDLE_INVALID;
    }
    if (sampler == SAMPLER_HANDLE_INVALID || sampler > s_textures.sampler_count)
    {
        Log(ERROR, "invalid sampler handle %u", sampler);
        return TEXTURE_HANDLE_INVALID;
    }

    texture_t texture = {
        .width = width,
        .height = height,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
    };

    VkImageLayout layout;
    if (!VulkanImage_CreateStatic(width, height, rgba_data, &texture.image,
                                  &texture.image_memory, &layout))
    {
        Log(ERROR, "failed to create texture image");
        return TEXTURE_HANDLE_INVALID;
    }

    if (!VulkanImage_CreateView(texture.image, texture.format, VK_IMAGE_ASPECT_COLOR_BIT, 1,
                                &texture.image_view))
    {
        Log(ERROR, "failed to create texture image view");
        vkDestroyImage(g_device, texture.image, NULL);
        vkFreeMemory(g_device, texture.image_memory, NULL);
        return TEXTURE_HANDLE_INVALID;
    }

    s_textures.textures[s_textures.texture_count++] = texture;
    texture_handle_t handle = (texture_handle_t)s_textures.texture_count; /* 1-based */

    VkDescriptorImageInfo image_info = {
        .sampler = s_textures.samplers[sampler - 1],
        .imageView = texture.image_view,
        .imageLayout = layout,
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = s_textures.descriptor_set,
        .dstBinding = BINDLESS_TEXTURE_BINDING,
        .dstArrayElement = handle,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .pImageInfo = &image_info,
    };

    vkUpdateDescriptorSets(g_device, 1, &write, 0, NULL);

    Log(INFO, "Created texture %u [%ux%u]", handle, width, height);

    return handle;
}

sampler_handle_t VulkanTexture_CreateSampler()
{
    if (s_textures.sampler_count >= MAX_SAMPLERS)
    {
        Log(ERROR, "maximum number of samplers reached");
        return SAMPLER_HANDLE_INVALID;
    }

    // TODO configurable filtering/addressing
    VkSamplerCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = true,
        .maxAnisotropy = 16.0f,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .maxLod = 1.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    };

    VkSampler sampler;
    if (vkCreateSampler(g_device, &create_info, NULL, &sampler) != VK_SUCCESS)
    {
        Log(ERROR, "failed to create sampler");
        return SAMPLER_HANDLE_INVALID;
    }

    s_textures.samplers[s_textures.sampler_count++] = sampler;

    return (sampler_handle_t)s_textures.sampler_count; /* 1-based */
}

VkDescriptorSetLayout VulkanTexture_GetDescriptorSetLayout()
{
    Assert(s_textures.descriptor_set_layout != VK_NULL_HANDLE);

    return s_textures.descriptor_set_layout;
}

VkDescriptorSet VulkanTexture_GetDescriptorSet()
{
    Assert(s_textures.descriptor_set != VK_NULL_HANDLE);

    return s_textures.descriptor_set;
}
