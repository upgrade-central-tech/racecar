#include "lighting_pass.hpp"

#include "../engine/draw_task.hpp"
#include "../engine/gfx_task.hpp"
#include "../engine/pipeline_barrier.hpp"
#include "../geometry/quad.hpp"
#include "../vk/create.hpp"

namespace racecar {

namespace {

constexpr std::string_view LIGHTING_PASS_SHADER_MODULE_PATH = "../shaders/deferred/lighting.spv";

}

void create_deferred_lighting_pipeline_barrier(
    engine::TaskList& task_list,
    deferred::GBuffers& gbuffers,
#if RACECAR_RAY_TRACING
    engine::RWImage& reflection_data,
#endif // RACECAR_RAY_TRACING
    engine::RWImage& screen_color
)
{
    engine::add_pipeline_barrier(
        task_list,
        engine::PipelineBarrierDescriptor {
            .buffer_barriers = { },
            .image_barriers = {
#if !RACECAR_RAY_TRACING
                deferred::color_write_to_frag_read( gbuffers.GBuffer_Position ),
                deferred::color_write_to_frag_read( gbuffers.GBuffer_Velocity ),
                deferred::color_write_to_frag_read( gbuffers.GBuffer_Normal ),
#endif // RACECAR_RAY_TRACING
                deferred::color_write_to_frag_read( gbuffers.GBuffer_Tangent ),
                deferred::color_write_to_frag_read( gbuffers.GBuffer_UV ),
                deferred::color_write_to_frag_read( gbuffers.GBuffer_Albedo ),
                deferred::color_write_to_frag_read( gbuffers.GBuffer_Packed_Data ),
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                                       .src_access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                       .src_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                       .dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                       .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                                       .dst_layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
                                       .image = &gbuffers.GBuffer_Depth,
                                       .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_DEPTH },
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                                       .src_access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                       .src_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                       .dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                       .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                                       .dst_layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
                                       .image = &gbuffers.GBuffer_DepthMS,
                                       .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_DEPTH },
#if RACECAR_RAY_TRACING
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                       .src_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                       .src_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       .dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                       .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                                       .dst_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                       .image = &reflection_data,
                                       .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR },
#endif // RACECAR_RAY_TRACING
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                       .src_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                       .src_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                       .dst_access = VK_ACCESS_2_SHADER_WRITE_BIT,
                                       .dst_layout = VK_IMAGE_LAYOUT_GENERAL,
                                       .image = &screen_color,
                                       .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR },
            } }
    );
}

void create_terrain_car_screen_pipeline_barrier(
    engine::TaskList& task_list, engine::RWImage& screen_color
)
{
    engine::add_pipeline_barrier(
        task_list,
        engine::PipelineBarrierDescriptor {
            .buffer_barriers = { },
            .image_barriers = { engine::ImageBarrier {
                .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .src_access = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .src_layout = VK_IMAGE_LAYOUT_GENERAL,
                .dst_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dst_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .dst_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .image = &screen_color,
                .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR } } }
    );
}

void create_lighting_pass_resources(
    LightingPassDescSets desc_sets,
    engine::Pipeline* lighting_pass_pipeline
)
{
    *lighting_pass_pipeline = engine::create_gfx_pipeline(
        engine::get_vertex_input_state_create_info( geometry::quad::Mesh::get_instance() ),
        { desc_sets.uniform_desc_set.layouts[0],
          desc_sets.material_desc_sets[0].layouts[0],
          desc_sets.lut_sets.layouts[0],
          desc_sets.sampler_desc_set.layouts[0],
          desc_sets.gbuffer_desc_set.layouts[0],
#if RACECAR_RAY_TRACING
          desc_sets.car_tlas_desc_set.layouts[0],
          desc_sets.reflection_buffer_desc_set.layouts[0]
#endif // RACECAR_RAY_TRACING
        },
        { VK_FORMAT_R16G16B16A16_SFLOAT },
        VK_SAMPLE_COUNT_1_BIT,
        true,
        false,
        vk::create::shader_module( LIGHTING_PASS_SHADER_MODULE_PATH ),
        false
    );
}

void car_lighting_pass(
    LightingPassDescSets desc_sets,
    engine::Pipeline& lighting_pass_pipeline,
    engine::RWImage& screen_color,
    engine::TaskList& task_list
)
{
    const engine::State& engine = engine::State::GetConst();
    geometry::quad::Mesh& quad_mesh = geometry::quad::Mesh::get_instance();

    engine::GfxTask lighting_pass_gfx_task = { .clear_depth = 1.0f,
                                               .color_attachments = { &screen_color },
                                               .extent = engine.swapchain.extent };

    lighting_pass_gfx_task.draw_tasks.push_back({
            .draw_resource_descriptor = {
                .vertex_buffers = { quad_mesh.mesh_buffers.vertex_buffer.handle },
                .index_buffer = quad_mesh.mesh_buffers.index_buffer.handle,
                .vertex_buffer_offsets = { 0 },
                .index_count = static_cast<uint32_t>( quad_mesh.indices.size() ),
            },
            .descriptor_sets = {
                &desc_sets.uniform_desc_set,
                &desc_sets.material_desc_sets[static_cast<size_t>( 0 )], // THIS IS WRONG; NEEDS FIX
                &desc_sets.lut_sets,
                &desc_sets.sampler_desc_set,
                &desc_sets.gbuffer_desc_set,
#if RACECAR_RAY_TRACING
                &desc_sets.car_tlas_desc_set,
                &desc_sets.reflection_buffer_desc_set
#endif // RACECAR_RAY_TRACING
            },
            .pipeline = lighting_pass_pipeline,
        });

    engine::add_gfx_task( task_list, lighting_pass_gfx_task );
}

}
