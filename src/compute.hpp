#pragma once

#include "engine/pipeline.hpp"

#include <glm/glm.hpp>

namespace racecar::engine {

struct ComputeCall {
    VkShaderModule compute_shader = VK_NULL_HANDLE;
    Pipeline pipeline;
};

struct ComputeTask {
    std::vector<ComputeCall> compute_calls;
};

std::optional<ComputeTask> create_compute_task();
void execute_compute_task();
void add_compute_call( const ComputeTask& compute_call );

std::optional<ComputeCall> create_compute_call( const VkShaderModule& compute_shader );
void dispatch_compute_call( const ComputeTask& task );

} // namespace racecar::engine
