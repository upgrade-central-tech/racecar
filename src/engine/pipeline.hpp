#pragma once

#include "../vk/common.hpp"
#include "state.hpp"

#include <optional>

namespace racecar::engine {

struct Pipeline {
    VkPipeline handle = nullptr;
    VkPipelineLayout layout = nullptr;
};

// this def needs to be refactored
Pipeline create_gfx_pipeline( const engine::State& engine, vk::Common& vulkan,
    std::optional<VkPipelineVertexInputStateCreateInfo> vertex_input_state_create_info,
    const std::vector<VkDescriptorSetLayout>& layouts, const std::vector<VkFormat> color_attachment_formats, VkShaderModule shader_module );

template <typename Mesh>
VkPipelineVertexInputStateCreateInfo get_vertex_input_state_create_info( const Mesh& mesh )
{
    return {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &mesh.vertex_binding_description,
        .vertexAttributeDescriptionCount
        = static_cast<uint32_t>( mesh.attribute_descriptions.size() ),
        .pVertexAttributeDescriptions = mesh.attribute_descriptions.data(),
    };
}

} // namespace racecar::engine
