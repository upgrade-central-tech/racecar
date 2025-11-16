#pragma once

#include <volk.h>
#include <vector>

namespace racecar::engine {

struct BufferBarrier {
    // the buffer to surround the barrier
    VkBuffer buffer;

    /// the stage that the previous commands must reach
    VkPipelineStageFlags2 src_stage;
    
    /// the data write type that must be available to stages after this
    VkAccessFlags2 src_access;

    /// the stage at which the following commands must wait
    VkPipelineStageFlags2 dst_stage;

    /// the data read type that the following commands will read it through
    VkAccessFlags2 dst_access;

    VkBufferMemoryBarrier2 get_vk();
};

struct ImageBarrier {
    /// the stage that the previous commands must reach
    VkPipelineStageFlags2 src_stage;

    /// the data write type that must be available to stages after this
    VkAccessFlags2 src_access;

    /// the layout of the image in the previous stage
    VkImageLayout src_layout;

    /// the stage at which the following commands must wait
    VkPipelineStageFlags2 dst_stage;

    /// the data read type that the following commands will read it through
    VkAccessFlags2 dst_access;
    
    /// the layout of the image we want to transition to
    VkImageLayout dst_layout;

    VkImage image;
    VkImageSubresourceRange range;

    VkImageMemoryBarrier2 get_vk();
};

struct PipelineBarrierDescriptor {
    std::vector<BufferBarrier> buffer_barriers;
    std::vector<ImageBarrier> image_barriers;
};

void run_pipeline_barrier( PipelineBarrierDescriptor barrier, VkCommandBuffer cmd_buf );

}
