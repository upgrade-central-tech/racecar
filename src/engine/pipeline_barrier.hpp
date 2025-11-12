#pragma once

#include <volk.h>

namespace racecar::engine {

struct PipelineBarrierDescriptor {
    VkPipelineStageFlags pre_stage;
    VkPipelineStageFlags post_stage;
};

void run_pipeline_barrier( PipelineBarrierDescriptor barrier, VkCommandBuffer cmd_buf );

}
