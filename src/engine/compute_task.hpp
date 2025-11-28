#pragma once

#include "descriptor_set.hpp"
#include "pipeline.hpp"

#include <glm/glm.hpp>

namespace racecar::engine {

struct ComputeTask {
    Pipeline pipeline;
    std::vector<engine::DescriptorSet*> descriptor_sets;

    glm::ivec3 group_size;
};

void execute_cs_task(
    const engine::State& engine, const VkCommandBuffer& cmd_buf, ComputeTask& compute_task );

} // namespace racecar::engine
