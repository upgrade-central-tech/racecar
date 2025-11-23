#include "compute.hpp"

namespace racecar::engine {

void execute_cs_task(const engine::State& engine, const VkCommandBuffer& cmd_buf, ComputeTask &compute_task) {
    vkCmdBindPipeline( cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, compute_task.pipeline.handle );

    for ( size_t i = 0; i < compute_task.descriptor_sets.size(); ++i ) {
        engine::DescriptorSet* descriptor_set = compute_task.descriptor_sets[i];

        vkCmdBindDescriptorSets( cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
            compute_task.pipeline.layout, static_cast<uint32_t>( i ), 1,
            &descriptor_set->descriptor_sets[engine.get_frame_index()], 0, nullptr );
    }

    vkCmdDispatch(cmd_buf, uint32_t(compute_task.group_size.x), uint32_t(compute_task.group_size.y), uint32_t(compute_task.group_size.z));
}

}