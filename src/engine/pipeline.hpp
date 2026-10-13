#pragma once

#include "../vk/common.hpp"
#include "state.hpp"

#include <optional>

namespace racecar::engine {

struct Pipeline {
    VkPipeline handle = nullptr;
    VkPipelineLayout layout = nullptr;
};

constexpr std::string_view DEFAULT_VERTEX_ENTRY_NAME = "vs_main";
constexpr std::string_view DEFAULT_TESS_CONTROL_ENTRY_NAME = "ts_control_main";
constexpr std::string_view DEFAULT_TESS_EVAL_ENTRY_NAME = "ts_eval_main";
constexpr std::string_view DEFAULT_FRAGMENT_ENTRY_NAME = "fs_main";

struct ShaderEntryPointStrings {
    const std::string_view vertex_entry_name;
    const std::string_view tess_control_entry_name;
    const std::string_view tess_eval_entry_name;
    const std::string_view fragment_entry_name;

    static ShaderEntryPointStrings defaults() {
        return ShaderEntryPointStrings {
            .vertex_entry_name = DEFAULT_VERTEX_ENTRY_NAME,
            .tess_control_entry_name = DEFAULT_TESS_CONTROL_ENTRY_NAME,
            .tess_eval_entry_name = DEFAULT_TESS_EVAL_ENTRY_NAME,
            .fragment_entry_name = DEFAULT_FRAGMENT_ENTRY_NAME
        };
    }
};

Pipeline create_gfx_pipeline(
    std::optional<VkPipelineVertexInputStateCreateInfo> vertex_input_state_create_info,
    const std::vector<VkDescriptorSetLayout>& layouts,
    const std::vector<VkFormat> color_attachment_formats, VkSampleCountFlagBits samples, bool blend,
    bool depth_test, VkShaderModule shader_module, ShaderEntryPointStrings shader_entry_point_strings, bool enable_tessellation_shaders, bool wireframe );

Pipeline create_compute_pipeline(
    const std::vector<VkDescriptorSetLayout>& layouts, VkShaderModule shader_module,
    std::string_view entry_name );

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
