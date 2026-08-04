#include "prepass.hpp"

#include "../vk/create.hpp"

namespace racecar::engine {

namespace {

constexpr std::string_view DEPTH_PREPASS_SHADER_MODULE_PATH
    = "../shaders/deferred/depth_prepass.spv";

}

void PushDepthPrepassMS(
    DepthPrepassMS& depth_prepass_ms, engine::DrawResourceDescriptor draw_descriptor
)
{
    depth_prepass_ms.depth_ms_gfx_task.draw_tasks.push_back(
        {
            .draw_resource_descriptor = draw_descriptor,
            .descriptor_sets = depth_prepass_ms.descriptor_sets,
            .pipeline = depth_prepass_ms.pipeline,
        }
    );
}

engine::GfxTask create_depth_ms_gfx_task( deferred::GBuffers* gbuffers )
{
    const engine::State& engine = engine::State::GetConst();
    return {
        .clear_color = { { { 0.0f, 0.0f, 0.0f, 0.0f } } },
        .clear_depth = 1.f,
        .color_attachments = { },
        .depth_image = &gbuffers->GBuffer_DepthMS,
        .extent = engine.swapchain.extent,
    };
}

void create_depth_ms_prepass(
    engine::DescriptorSet* depth_uniform_desc_set,
    engine::Pipeline* depth_ms_pipeline,
    const UniformBuffer<ub_data::Camera>& camera_buffer,
    const geometry::scene::Mesh& scene_mesh,
    deferred::GBuffers* gbuffers,
    engine::DepthPrepassMS* depth_prepass_ms
)
{
    const engine::State& engine = engine::State::GetConst();
    *depth_uniform_desc_set = engine::generate_descriptor_set(
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
        VK_SHADER_STAGE_VERTEX_BIT
    );

    engine::update_descriptor_set_uniform( *depth_uniform_desc_set, camera_buffer, 0 );

    // DEPTH_MS PRE-PASS
    try {
        // Pipeline needs to support MSAA
        size_t frame_index = engine.get_frame_index();
        *depth_ms_pipeline = create_gfx_pipeline(
            VkPipelineVertexInputStateCreateInfo {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &scene_mesh.vertex_binding_description,
                .vertexAttributeDescriptionCount = 1,
                .pVertexAttributeDescriptions = scene_mesh.attribute_descriptions.data(),
            },
            { depth_uniform_desc_set->layouts[frame_index] },
            { },
            VK_SAMPLE_COUNT_4_BIT,
            false,
            true,
            vk::create::shader_module( DEPTH_PREPASS_SHADER_MODULE_PATH ),
            false
        );
    } catch ( const Exception& ex ) {
        log::error( "Failed to create depth-MS-prepass pipeline: {}", ex.what() );
        throw;
    }

    depth_prepass_ms->depth_ms_gfx_task = create_depth_ms_gfx_task( gbuffers );
    depth_prepass_ms->descriptor_sets = { depth_uniform_desc_set };
    depth_prepass_ms->pipeline = *depth_ms_pipeline;
}

}
