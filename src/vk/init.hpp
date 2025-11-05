#pragma once

#include "common.hpp"

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

#include <filesystem>
#include <optional>
#include <string_view>

namespace racecar::vk::init {

VkCommandPoolCreateInfo command_pool_create_info(uint32_t queue_family_index,
                                                 VkCommandPoolCreateFlags flags);

VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, uint32_t count);
VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags);

VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags);
VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags);

VkImageSubresourceRange image_subresource_range(VkImageAspectFlags aspect_mask);

VkSemaphoreSubmitInfo semaphore_submit_info(VkPipelineStageFlags2 stage_mask,
                                            VkSemaphore semaphore);
VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer command_buffer);
VkSubmitInfo2 submit_info(VkCommandBufferSubmitInfo* command_buffer_info,
                          VkSemaphoreSubmitInfo* signal_semaphore_info,
                          VkSemaphoreSubmitInfo* wait_semaphore_info);

VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(VkShaderStageFlagBits flags,
                                                                  VkShaderModule shader_module,
                                                                  std::string_view name);

/// TODO: This is a util function, we need to move this!
void transition_image(VkCommandBuffer command_buffer,
                      VkImage image,
                      VkImageLayout currentLayout,
                      VkImageLayout newLayout);

/// Creates shader module from the specified file path. Path is relative to the executable.
std::optional<VkShaderModule> create_shader_module(const Common& vulkan,
                                                   std::filesystem::path shader_path);

}  // namespace racecar::vk::init
