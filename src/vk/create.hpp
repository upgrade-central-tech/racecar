#pragma once

#include "common.hpp"

#include <SDL3/SDL_vulkan.h>

#include <filesystem>
#include <string_view>

namespace racecar::vk::create {

VkCommandPoolCreateInfo command_pool_info(
    uint32_t queue_family_index, VkCommandPoolCreateFlags flags );

VkCommandBufferAllocateInfo command_buffer_allocate_info( VkCommandPool pool, uint32_t count );
VkCommandBufferBeginInfo command_buffer_begin_info( VkCommandBufferUsageFlags flags );

VkFenceCreateInfo fence_info( VkFenceCreateFlags flags );
VkSemaphoreCreateInfo semaphore_info();

VkImageSubresourceRange image_subresource_range( VkImageAspectFlags aspect_mask );

VkImageCreateInfo image_info(
    VkFormat format, VkImageType image_type, VkImageUsageFlags usage_flags, VkExtent3D extent );
VkImageViewCreateInfo image_view_info(
    VkFormat format, VkImage image, VkImageViewType image_view, VkImageAspectFlags aspect_flags );

VkSemaphoreSubmitInfo semaphore_submit_info(
    VkPipelineStageFlags2 stage_mask, VkSemaphore semaphore );
VkCommandBufferSubmitInfo command_buffer_submit_info( VkCommandBuffer command_buffer );
VkSubmitInfo2 submit_info( VkCommandBufferSubmitInfo* command_buffer_info,
    VkSemaphoreSubmitInfo* signal_semaphore_info, VkSemaphoreSubmitInfo* wait_semaphore_info );

VkPipelineShaderStageCreateInfo pipeline_shader_stage_info(
    VkShaderStageFlagBits flags, VkShaderModule shader_module, std::string_view name );

/// Creates shader module from the specified file path. Path is relative to the executable.
///
/// Note that the destruction of the shader module is pushed into the destructor stack. Depending on
/// what we want (e.g. destroying after pipeline creation) this may not be what we want.
VkShaderModule shader_module( Common& vulkan, std::filesystem::path shader_path );

struct CreateSubmitInfoDescriptor {
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkSemaphore wait_semaphore = VK_NULL_HANDLE;
    VkPipelineStageFlagBits2 wait_flag_bits = VK_PIPELINE_STAGE_2_NONE;
    VkSemaphore signal_semaphore = VK_NULL_HANDLE;
    VkPipelineStageFlagBits2 signal_flag_bits = VK_PIPELINE_STAGE_2_NONE;
};

struct AllSubmitInfo {
    VkSemaphoreSubmitInfo wait_info = {};
    VkSemaphoreSubmitInfo signal_info = {};
    VkCommandBufferSubmitInfo command_info = {};
};

AllSubmitInfo all_submit_info( CreateSubmitInfoDescriptor submit_info_descriptor );
VkSubmitInfo2 submit_info_from_all( AllSubmitInfo& all_submit_info );

} // namespace racecar::vk::create
