#pragma once

#include "../vk/common.hpp"
#include "state.hpp"

namespace racecar::engine {

struct Pipeline {
    VkPipeline handle = nullptr;
    VkPipelineLayout layout = nullptr;

    /// TODO: Add support functions for things like disabling MSAA, etc.
    /// TODO: Need disable depth test function, etc.
};

/// Creates the graphics pipeline, if successful. Remember to call `free_pipeline` to destroy
/// the layout and other potential objects.
std::optional<Pipeline> create_gfx_pipeline( const engine::State& engine,
                                             const vk::Common& vulkan,
                                             VkShaderModule shader_module );
void free_pipeline( const vk::Common& vulkan, Pipeline& pipeline );

}  // namespace racecar::engine
