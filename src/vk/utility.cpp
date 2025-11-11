#include "utility.hpp"

#include "create.hpp"

namespace racecar::vk::utility {

void transition_image( VkCommandBuffer command_buffer,
                       VkImage image,
                       VkImageLayout old_layout,
                       VkImageLayout new_layout,
                       VkAccessFlags2 src_access_mask,
                       VkAccessFlags2 dst_access_mask,
                       VkPipelineStageFlags2 src_stage_mask,
                       VkPipelineStageFlags2 dst_stage_mask ) {
    VkImageAspectFlags aspect_mask = new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
                                         ? VK_IMAGE_ASPECT_DEPTH_BIT
                                         : VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageMemoryBarrier2 image_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

        // For example, we can consider only looking at COMPUTE work, so
        // VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT.
        // Similarly, dstStageMask represents the work that waits until the barrier is
        // lifted.
        //
        // Note: VkGuide notes that ALL_COMMANDS in StageMask is inefficient!!!
        // More refined examples of using StageMask effectively can be found here:
        // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
        .srcStageMask = src_stage_mask,
        .srcAccessMask = src_access_mask,
        .dstStageMask = dst_stage_mask,
        .dstAccessMask = dst_access_mask,

        .oldLayout = old_layout,
        .newLayout = new_layout,

        .image = image,
        .subresourceRange = vk::create::image_subresource_range( aspect_mask ),
    };

    VkDependencyInfo dependency_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &image_barrier,
    };

    vkCmdPipelineBarrier2( command_buffer, &dependency_info );
}

/// Open to ideas for how to replace this.
uint32_t bytes_from_format( VkFormat format ) {
    switch ( format ) {
        case VK_FORMAT_R8_UNORM:
            return 1;

        case VK_FORMAT_R8G8B8_UNORM:
            return 3;

        case VK_FORMAT_R8G8B8A8_UNORM:
            return 4;

        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return 16;

        default:
            return 0;
    }
}

}  // namespace racecar::vk::utility
