#include "pipeline_barrier.hpp"

namespace racecar {

void run_pipeline_barrier( PipelineBarrierDescriptor barrier, VkCommandBuffer cmd_buf ) {
    vkCmdPipelineBarrier( cmd_buf, barrier.pre_stage, barrier.post_stage, 0, 0, VK_NULL_HANDLE, 0,
                          VK_NULL_HANDLE, 0, VK_NULL_HANDLE );
}

}  // namespace racecar
