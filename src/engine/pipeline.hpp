#pragma once

#include "../geometry/mesh.hpp"
#include "../vk/common.hpp"
#include "state.hpp"

#include <optional>

namespace racecar::engine {

struct Pipeline {
    VkPipeline handle = nullptr;
    VkPipelineLayout layout = nullptr;
};

Pipeline create_gfx_pipeline( const engine::State& engine, vk::Common& vulkan,
    const std::optional<const geometry::Mesh>& mesh,
    const std::vector<VkDescriptorSetLayout>& layouts, VkShaderModule shader_module );

} // namespace racecar::engine
