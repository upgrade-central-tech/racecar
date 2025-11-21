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

ComputeTask create_compute_task();
void execute_compute_task();
void add_compute_call( const ComputeTask& compute_call );

} // namespace racecar::engine
