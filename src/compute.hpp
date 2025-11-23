#pragma once

#include "engine/descriptor_set.hpp"
#include "engine/pipeline.hpp"

#include <glm/glm.hpp>

namespace racecar::engine {

struct ComputeTask {
    Pipeline pipeline;
    std::vector<DescriptorSet*> descriptor_sets;

    glm::ivec3 group_size;
};

void execute_compute_task(ComputeTask &compute_task);

} // namespace racecar::engine
