#pragma once

#include "common.hpp"

#include <SDL3/SDL_vulkan.h>

#include <filesystem>
#include <optional>
#include <string_view>

namespace racecar::vk::create {

VkCommandPoolCreateInfo command_pool_info( uint32_t queue_family_index,
                                           VkCommandPoolCreateFlags flags );

VkCommandBufferAllocateInfo command_buffer_allocate_info( VkCommandPool pool, uint32_t count );
VkCommandBufferBeginInfo command_buffer_begin_info( VkCommandBufferUsageFlags flags );

VkFenceCreateInfo fence_info( VkFenceCreateFlags flags );
VkSemaphoreCreateInfo semaphore_info();

VkImageSubresourceRange image_subresource_range( VkImageAspectFlags aspect_mask );

VkSemaphoreSubmitInfo semaphore_submit_info( VkPipelineStageFlags2 stage_mask,
                                             VkSemaphore semaphore );
VkCommandBufferSubmitInfo command_buffer_submit_info( VkCommandBuffer command_buffer );
VkSubmitInfo2 submit_info( VkCommandBufferSubmitInfo* command_buffer_info,
                           VkSemaphoreSubmitInfo* signal_semaphore_info,
                           VkSemaphoreSubmitInfo* wait_semaphore_info );

VkPipelineShaderStageCreateInfo pipeline_shader_stage_info( VkShaderStageFlagBits flags,
                                                            VkShaderModule shader_module,
                                                            std::string_view name );

/// Creates shader module from the specified file path. Path is relative to the executable.
std::optional<VkShaderModule> shader_module( const Common &vulkan,
                                             std::filesystem::path shader_path );

struct CreateSubmitInfoDescriptor {
    VkCommandBuffer command_buffer;
    VkSemaphore wait_semaphore;
    VkPipelineStageFlagBits2 wait_flag_bits;
    VkSemaphore signal_semaphore;
    VkPipelineStageFlagBits2 signal_flag_bits;
};

struct AllSubmitInfo {
    VkSemaphoreSubmitInfo wait_info;
    VkSemaphoreSubmitInfo signal_info;
    VkCommandBufferSubmitInfo command_info;
};

AllSubmitInfo all_submit_info(CreateSubmitInfoDescriptor submit_info_descriptor);
VkSubmitInfo2 submit_info_from_all( AllSubmitInfo &all_submit_info );

} // namespace racecar::vk::create
