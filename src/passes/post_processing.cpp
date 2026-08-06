#include "post_processing.hpp"

#include "../engine/pipeline_barrier.hpp"

namespace racecar {

// Convert the screen_color/rendered image to read-only.
// Output screen buffer must be converted to write-only.
// These are special pipeline barriers, since it is assumed that
// the scene color was rendered to via gfx draw calls, hence the attachment.
// We may need future helpers to convert from attachment to cs write, etc. and vice versa.
//
// Current limitation assumes that all post-processing calls are done via compute shader
// hence the VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT stage.
void create_screen_buffer_pipeline_barrier(
    engine::RWImage& screen_color,
    engine::RWImage& screen_buffer,
    engine::RWImage& gbuffer_depth,
    engine::TaskList& task_list
)
{
    engine::add_pipeline_barrier(
        task_list,
        engine::PipelineBarrierDescriptor {
            .buffer_barriers = { },
            .image_barriers = {
                engine::ImageBarrier {
                    .src_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .src_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .image = &screen_color,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
                },
                engine::ImageBarrier {
                    .src_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .src_access = VK_ACCESS_2_NONE,
                    .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .dst_access = VK_ACCESS_2_SHADER_WRITE_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_GENERAL,
                    .image = &screen_buffer,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
                },
                engine::ImageBarrier {
                    .src_stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                    .src_access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
                    .image = &gbuffer_depth,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_DEPTH,
                },
            } }
    );
}

void create_screen_buffer_present_pipeline_barrier(
    engine::RWImage& screen_buffer, engine::TaskList& task_list
)
{
    engine::add_pipeline_barrier(
        task_list,
        engine::PipelineBarrierDescriptor {
            .buffer_barriers = { },
            .image_barriers = {
                engine::ImageBarrier {
                    .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .src_access = VK_ACCESS_2_SHADER_WRITE_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_GENERAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .dst_access = VK_ACCESS_2_TRANSFER_READ_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .image = &screen_buffer,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
                },
            } }
    );
}

void post_processing_passes(
    UniformBuffer<ub_data::Camera>& camera_buffer,
    deferred::GBuffers& gbuffers,
    engine::RWImage& screen_color,
    engine::RWImage& screen_buffer,
    engine::RWImage& screen_history,
    engine::TaskList& task_list,
    engine::post::AAPass& aa_pass,
    engine::post::AoPass& ao_pass,
    engine::post::BloomPass& bloom_pass,
    engine::post::TonemappingPass& tm_pass
)
{
    engine::post::add_bloom( &bloom_pass, task_list, screen_color, screen_buffer );

    ao_pass = {
        .camera_buffer = &camera_buffer,
        .GBuffer_Normal = &gbuffers.GBuffer_Normal,
        .GBuffer_Depth = &gbuffers.GBuffer_Depth,
        .in_color = &screen_color,
        .out_color = &screen_buffer,
    };

    engine::add_pipeline_barrier(
        task_list,
        engine::PipelineBarrierDescriptor {
            .buffer_barriers = { },
            .image_barriers = { engine::ImageBarrier {
                .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .src_access = VK_ACCESS_2_SHADER_READ_BIT,
                .src_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dst_access = VK_ACCESS_2_SHADER_WRITE_BIT,
                .dst_layout = VK_IMAGE_LAYOUT_GENERAL,
                .image = &screen_buffer,
                .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
            } },
        }
    );
    add_ao( ao_pass, task_list );

    engine::add_pipeline_barrier(
        task_list,
        engine::PipelineBarrierDescriptor {
            .buffer_barriers = { },
            .image_barriers = {
                engine::ImageBarrier {
                    .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .src_access = VK_ACCESS_2_SHADER_READ_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .dst_access = VK_ACCESS_2_SHADER_WRITE_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_GENERAL,
                    .image = &screen_color,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
                },
                engine::ImageBarrier {
                    .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .src_access = VK_ACCESS_2_SHADER_WRITE_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_GENERAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .image = &screen_buffer,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
                },
            } }
    );
    tm_pass = engine::post::add_tonemapping( screen_buffer, screen_color, task_list );

    engine::add_pipeline_barrier(
        task_list,
        engine::PipelineBarrierDescriptor {
            .buffer_barriers = { },
            .image_barriers = {
                engine::ImageBarrier {
                    .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .src_access = VK_ACCESS_2_SHADER_READ_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .dst_access = VK_ACCESS_2_SHADER_WRITE_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_GENERAL,
                    .image = &screen_buffer,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
                },
                engine::ImageBarrier {
                    .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .src_access = VK_ACCESS_2_SHADER_WRITE_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_GENERAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .image = &screen_color,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
                },
            } }
    );
    aa_pass = engine::post::add_aa(
        screen_color,
        gbuffers.GBuffer_Depth,
        gbuffers.GBuffer_Velocity,
        screen_buffer,
        screen_history,
        task_list,
        camera_buffer
    );
}

}
