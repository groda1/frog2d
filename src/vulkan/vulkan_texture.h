#ifndef VULKAN_TEXTURE_H
#define VULKAN_TEXTURE_H

#include <vulkan/vulkan_core.h>

#include "core.h"

#include "render_types.h"

bool VulkanTexture_Init();
void VulkanTexture_Destroy();

/* rgba_data is copied to the device during the call (width * height * 4
   bytes); the returned handle is the texture's index in the global
   descriptor array */
texture_handle_t VulkanTexture_Create(u32 width, u32 height, const u8 *rgba_data,
                                      sampler_handle_t sampler);

/* a texture an image pass renders into; sampled like any other texture */
texture_handle_t VulkanTexture_CreateRenderTarget(u32 width, u32 height,
                                                  sampler_handle_t sampler);

sampler_handle_t VulkanTexture_CreateSampler();

VkImage     VulkanTexture_GetImage(texture_handle_t handle);
VkImageView VulkanTexture_GetImageView(texture_handle_t handle);
VkFormat    VulkanTexture_GetFormat(texture_handle_t handle);
VkExtent2D  VulkanTexture_GetExtent(texture_handle_t handle);

/* global bindless texture array, set 0 in every pipeline layout */
VkDescriptorSetLayout VulkanTexture_GetDescriptorSetLayout();
VkDescriptorSet       VulkanTexture_GetDescriptorSet();

#endif
