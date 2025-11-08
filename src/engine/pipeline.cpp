#include "pipeline.hpp"

#include "../vk/create.hpp"
#include "state.hpp"

#include <array>

namespace racecar::engine {

constexpr std::string_view VERTEX_ENTRY_NAME = "vs_main";
constexpr std::string_view FRAGMENT_ENTRY_NAME = "fs_main";

std::optional<Pipeline> create_gfx_pipeline( const engine::State& engine,
                                             const vk::Common& vulkan,
                                             const std::optional<const geometry::Mesh>& mesh,
                                             VkShaderModule shader_module ) {
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = nullptr,
    };

    if ( mesh.has_value() && mesh->mesh_buffers.vertex_buffer_address ) {
        vertex_input_info.vertexBindingDescriptionCount = 1,
        vertex_input_info.pVertexBindingDescriptions = &mesh->vertex_binding_description;
        vertex_input_info.vertexAttributeDescriptionCount =
            static_cast<uint32_t>( mesh->attribute_descriptions.size() );
        vertex_input_info.pVertexAttributeDescriptions = mesh->attribute_descriptions.data();
    }

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    std::array<VkDynamicState, 2> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>( dynamic_states.size() ),
        .pDynamicStates = dynamic_states.data(),
    };

    VkPipelineViewportStateCreateInfo viewport_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterization_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
    };

    VkPipelineMultisampleStateCreateInfo multisample_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState color_blend_attachment_info = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT };

    VkPipelineColorBlendStateCreateInfo color_blend_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment_info,
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };

    VkPipelineLayout gfx_layout = nullptr;

    if ( VkResult result =
             vkCreatePipelineLayout( vulkan.device, &pipeline_layout_info, nullptr, &gfx_layout );
         result != VK_SUCCESS ) {
        SDL_Log( "[Vulkan] Failed to create pipeline layout | Error code: %d", result );
        vkDestroyShaderModule( vulkan.device, shader_module, nullptr );
        return {};
    }

    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {
        vk::create::pipeline_shader_stage_info( VK_SHADER_STAGE_VERTEX_BIT, shader_module,
                                                VERTEX_ENTRY_NAME ),
        vk::create::pipeline_shader_stage_info( VK_SHADER_STAGE_FRAGMENT_BIT, shader_module,
                                                FRAGMENT_ENTRY_NAME ),
    };

    // Use dynamic rendering instead of manually creating render passes
    VkPipelineRenderingCreateInfo pipeline_rendering_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &engine.swapchain.image_format,
    };

    VkGraphicsPipelineCreateInfo gfx_pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &pipeline_rendering_info,
        .stageCount = 2,
        .pStages = shader_stages.data(),
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly_info,
        .pViewportState = &viewport_state_info,
        .pRasterizationState = &rasterization_info,
        .pMultisampleState = &multisample_info,
        .pDepthStencilState = &depth_stencil_info,
        .pColorBlendState = &color_blend_info,
        .pDynamicState = &dynamic_state_info,
        .layout = gfx_layout,
        .renderPass = nullptr,
    };

    Pipeline gfx_pipeline;

    RACECAR_VK_CHECK( vkCreateGraphicsPipelines( vulkan.device, nullptr, 1, &gfx_pipeline_info,
                                                 nullptr, &gfx_pipeline.handle ),
                      "Failed to create graphics pipeline" );

    vkDestroyShaderModule( vulkan.device, shader_module, nullptr );

    gfx_pipeline.layout = gfx_layout;

    return gfx_pipeline;
}

void free_pipeline( const vk::Common& vulkan, Pipeline& pipeline ) {
    vkDestroyPipeline( vulkan.device, pipeline.handle, nullptr );
    vkDestroyPipelineLayout( vulkan.device, pipeline.layout, nullptr );
}

}  // namespace racecar::engine
