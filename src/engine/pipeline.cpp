#include "pipeline.hpp"

#include "../vk/create.hpp"
#include "state.hpp"

#include <array>

namespace racecar::engine {

constexpr std::string_view VERTEX_ENTRY_NAME = "vs_main";
constexpr std::string_view FRAGMENT_ENTRY_NAME = "fs_main";

Pipeline create_gfx_pipeline( const engine::State& engine, vk::Common& vulkan,
    std::optional<VkPipelineVertexInputStateCreateInfo> vertex_input_state_create_info,
    const std::vector<VkDescriptorSetLayout>& layouts, const std::vector<VkFormat> color_attachment_formats, bool blend, VkShaderModule shader_module )
{
    if (color_attachment_formats.empty()) {
        throw Exception("[Pipeline] create_gfx_pipeline() called with no color attachment formats!");
    }
    
    VkPipelineVertexInputStateCreateInfo vertex_input_info
        = vertex_input_state_create_info.value_or(
          VkPipelineVertexInputStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO } );

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
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .minDepthBounds = 0.f,
        .maxDepthBounds = 1.f,
    };

    VkPipelineMultisampleStateCreateInfo multisample_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState color_blend_attachment_info = {
        .blendEnable = blend ? VK_TRUE : VK_FALSE,
        .srcColorBlendFactor = blend ? VK_BLEND_FACTOR_SRC_ALPHA : VkBlendFactor::VK_BLEND_FACTOR_ZERO,
        .dstColorBlendFactor = blend ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VkBlendFactor::VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    std::vector<VkPipelineColorBlendAttachmentState> color_attachment_infos(color_attachment_formats.size(), color_blend_attachment_info);

    VkPipelineColorBlendStateCreateInfo color_blend_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = uint32_t(color_attachment_formats.size()),
        .pAttachments = color_attachment_infos.data(),
    };

    Pipeline gfx_pipeline;

    {
        VkPipelineLayoutCreateInfo pipeline_layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        };

        if ( layouts.size() > 0 ) {
            pipeline_layout_info.setLayoutCount = static_cast<uint32_t>( layouts.size() );
            pipeline_layout_info.pSetLayouts = layouts.data();
        }

        vk::check( vkCreatePipelineLayout(
                       vulkan.device, &pipeline_layout_info, nullptr, &gfx_pipeline.layout ),
            "Failed to create graphics pipeline layout" );
        vulkan.destructor_stack.push( vulkan.device, gfx_pipeline.layout, vkDestroyPipelineLayout );
    }

    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {
        vk::create::pipeline_shader_stage_info(
            VK_SHADER_STAGE_VERTEX_BIT, shader_module, VERTEX_ENTRY_NAME ),
        vk::create::pipeline_shader_stage_info(
            VK_SHADER_STAGE_FRAGMENT_BIT, shader_module, FRAGMENT_ENTRY_NAME ),
    };

    // Use dynamic rendering instead of manually creating render passes
    VkPipelineRenderingCreateInfo pipeline_rendering_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = uint32_t(color_attachment_formats.size()),
        .pColorAttachmentFormats = color_attachment_formats.data(),
        .depthAttachmentFormat = engine.depth_images[0].image_format,
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
        .layout = gfx_pipeline.layout,
        .renderPass = nullptr,
    };

    vk::check( vkCreateGraphicsPipelines(
                   vulkan.device, nullptr, 1, &gfx_pipeline_info, nullptr, &gfx_pipeline.handle ),
        "Failed to create graphics pipeline" );
    vulkan.destructor_stack.push( vulkan.device, gfx_pipeline.handle, vkDestroyPipeline );

    return gfx_pipeline;
}

Pipeline create_compute_pipeline( vk::Common& vulkan,
    const std::vector<VkDescriptorSetLayout>& layouts, VkShaderModule shader_module,
    std::string_view entry_name )
{
    Pipeline compute_pipeline;

    {
        VkPipelineLayoutCreateInfo pipeline_info
            = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                  .setLayoutCount = static_cast<uint32_t>( layouts.size() ),
                  .pSetLayouts = layouts.data() };

        vkCreatePipelineLayout( vulkan.device, &pipeline_info, nullptr, &compute_pipeline.layout );
        vulkan.destructor_stack.push(
            vulkan.device, compute_pipeline.layout, vkDestroyPipelineLayout );
    }

    VkPipelineLayoutCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };

    if ( layouts.size() > 0 ) {
        pipeline_info.setLayoutCount = static_cast<uint32_t>( layouts.size() );
        pipeline_info.pSetLayouts = layouts.data();
    }

    VkPipelineLayout pipeline_layout;
    vkCreatePipelineLayout( vulkan.device, &pipeline_info, nullptr, &pipeline_layout );

    VkComputePipelineCreateInfo create_pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .layout = pipeline_layout,
    };

    create_pipeline_info.stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
        VK_SHADER_STAGE_COMPUTE_BIT, shader_module, entry_name.data(), nullptr };

    VkPipeline compute_pipeline_handle;
    vk::check( vkCreateComputePipelines( vulkan.device, VK_NULL_HANDLE, 1, &create_pipeline_info,
                   nullptr, &compute_pipeline_handle ),
        "Failed to create compute pipeline" );

    vulkan.destructor_stack.push( vulkan.device, compute_pipeline_handle, vkDestroyPipeline );

    compute_pipeline.handle = compute_pipeline_handle;
    compute_pipeline.layout = pipeline_layout;

    return compute_pipeline;
}

} // namespace racecar::engine
