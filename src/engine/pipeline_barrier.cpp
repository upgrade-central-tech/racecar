#include "pipeline_barrier.hpp"
#include <algorithm>
#include <iterator>

namespace racecar::engine {

void run_pipeline_barrier( PipelineBarrierDescriptor barrier, VkCommandBuffer cmd_buf )
{
    std::vector<VkBufferMemoryBarrier2> vk_buffer_barriers;
    std::vector<VkImageMemoryBarrier2> vk_image_barriers;

    std::transform(
        barrier.buffer_barriers.begin(),
        barrier.buffer_barriers.end(),
        std::back_inserter(vk_buffer_barriers),
        [](BufferBarrier b) { return b.get_vk(); }
    );

    std::transform(
        barrier.image_barriers.begin(),
        barrier.image_barriers.end(),
        std::back_inserter(vk_image_barriers),
        [](ImageBarrier b) { return b.get_vk(); }
    );

    VkDependencyInfo info {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = VK_NULL_HANDLE,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        .memoryBarrierCount = 0,
        .pMemoryBarriers = nullptr,
        .bufferMemoryBarrierCount = uint32_t(vk_buffer_barriers.size()),
        .pBufferMemoryBarriers = vk_buffer_barriers.data(),
        .imageMemoryBarrierCount = uint32_t(vk_image_barriers.size()),
        .pImageMemoryBarriers = vk_image_barriers.data()
    };

    vkCmdPipelineBarrier2( cmd_buf, &info );
}


VkBufferMemoryBarrier2 BufferBarrier::get_vk() {
    return VkBufferMemoryBarrier2 {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .pNext = VK_NULL_HANDLE,
        .srcStageMask = src_stage,
        .srcAccessMask = src_access,
        .dstStageMask = src_stage,
        .dstAccessMask = dst_access,
        .buffer = buffer,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .offset = 0,
        .size = VK_WHOLE_SIZE
    };
}

VkImageMemoryBarrier2 ImageBarrier::get_vk() {
    return VkImageMemoryBarrier2 {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = VK_NULL_HANDLE,
        .srcStageMask = src_stage,
        .srcAccessMask = src_access,
        .dstStageMask = dst_stage,
        .dstAccessMask = dst_access,
        .oldLayout = src_layout,
        .newLayout = dst_layout,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = range
    };
}

} // namespace racecar
