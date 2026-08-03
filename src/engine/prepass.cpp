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

engine::GfxTask create_depth_ms_gfx_task( engine::State& engine, deferred::GBuffers* gbuffers )
{
    return {
        .clear_color = { { { 0.0f, 0.0f, 0.0f, 0.0f } } },
        .clear_depth = 1.f,
        .color_attachments = { },
        .depth_image = &gbuffers->GBuffer_DepthMS,
        .extent = engine.swapchain.extent,
    };
}

void create_depth_ms_prepass(
    Context& ctx,
    engine::State& engine,
    engine::DescriptorSet* depth_uniform_desc_set,
    engine::Pipeline* depth_ms_pipeline,
    const UniformBuffer<ub_data::Camera>& camera_buffer,
    const geometry::scene::Mesh& scene_mesh,
    deferred::GBuffers* gbuffers,
    engine::DepthPrepassMS* depth_prepass_ms
)
{
    *depth_uniform_desc_set = engine::generate_descriptor_set(
        ctx.vulkan,
        engine,
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
        VK_SHADER_STAGE_VERTEX_BIT
    );

    engine::update_descriptor_set_uniform(
        ctx.vulkan,
        engine,
        *depth_uniform_desc_set,
        camera_buffer,
        0
    );

    // DEPTH_MS PRE-PASS
    try {
        // Pipeline needs to support MSAA
        size_t frame_index = engine.get_frame_index();
        *depth_ms_pipeline = create_gfx_pipeline(
            engine,
            ctx.vulkan,
            engine::get_vertex_input_state_create_info( scene_mesh ),
            { depth_uniform_desc_set->layouts[frame_index] },
            { },
            VK_SAMPLE_COUNT_4_BIT,
            false,
            true,
            vk::create::shader_module( ctx.vulkan, DEPTH_PREPASS_SHADER_MODULE_PATH ),
            false
        );
    } catch ( const Exception& ex ) {
        log::error( "Failed to create depth-MS-prepass pipeline: {}", ex.what() );
        throw;
    }

    depth_prepass_ms->depth_ms_gfx_task = create_depth_ms_gfx_task( engine, gbuffers );
    depth_prepass_ms->descriptor_sets = { depth_uniform_desc_set };
    depth_prepass_ms->pipeline = *depth_ms_pipeline;
}

}
