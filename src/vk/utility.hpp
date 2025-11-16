#pragma once

#include <volk.h>

namespace racecar::vk::utility {

/// Similar to a resource barrier transition in DX12, this is meant to adjust a given image's
/// layout/access properties, etc. For example, to write to our image while drawing, we need our
/// image layout to transition from a read only state (VK_IMAGE_LAYOUT_GENERAL) to write
/// (VK_IMAGE_LAYOUT_PRESENT_SRC_KHR).
///
/// Helpful execution and memory barrier info can be found here:
/// https://themaister.net/blog/2019/08/14/yet-another-blog-explaining-vulkan-synchronization
void transition_image( VkCommandBuffer command_buffer, VkImage image, VkImageLayout old_layout,
    VkImageLayout new_layout, VkAccessFlags2 src_access_mask, VkAccessFlags2 dst_access_mask,
    VkPipelineStageFlags2 src_stage_mask, VkPipelineStageFlags2 dst_stage_mask,
    VkImageAspectFlags aspect_flags );

uint32_t bytes_from_format( VkFormat format );

uint16_t float_to_half( float f );

} // namespace racecar::vk::utility
