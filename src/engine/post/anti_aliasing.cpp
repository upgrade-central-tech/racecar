#include "anti_aliasing.hpp"

#include "../../settings.h"
#include "../../vk/create.hpp"

#include <string_view>

namespace racecar::engine::post {

static constexpr std::string_view ANTI_ALIASING_SHADER_PATH = "../shaders/post/aa/aa.spv";
static constexpr std::string_view HISTORY_SHADER_PATH = "../shaders/post/aa/history_aa.spv";

AAPass add_aa(
    const RWImage& input,
    const RWImage& GBuffer_Depth,
    const RWImage& GBuffer_Velocity,
    RWImage& output,
    RWImage& history,
    TaskList& task_list,
    UniformBuffer<ub_data::Camera>& camera_buffer
)
{
    const vk::Common& vulkan = vk::Common::GetConst();
    const engine::State& engine = engine::State::GetConst();
    AAPass pass;
    {
        pass.buffer = create_uniform_buffer<ub_data::AA>( { }, engine.frame_overlap );

        engine::DescriptorSet uniform_desc_set = engine::generate_descriptor_set(
            {
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // scene_color
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // scene_history
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // depth
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // velocity
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //  out
                VK_DESCRIPTOR_TYPE_SAMPLER, // sampler
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // camera data
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // aa_data
            },
            VK_SHADER_STAGE_COMPUTE_BIT
        );

        engine::update_descriptor_set_rwimage(
            uniform_desc_set,
            input,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            0
        );
        engine::update_descriptor_set_rwimage(
            uniform_desc_set,
            history,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            1
        );
        engine::update_descriptor_set_depth_image( uniform_desc_set, GBuffer_Depth, 2 );
        engine::update_descriptor_set_rwimage(
            uniform_desc_set,
            GBuffer_Velocity,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            3
        );
        engine::update_descriptor_set_rwimage(
            uniform_desc_set,
            output,
            VK_IMAGE_LAYOUT_GENERAL,
            4
        );
        engine::update_descriptor_set_sampler(
            uniform_desc_set,
            vulkan.global_samplers.linear_sampler,
            5
        );
        engine::update_descriptor_set_uniform( uniform_desc_set, camera_buffer, 6 );
        engine::update_descriptor_set_uniform( uniform_desc_set, pass.buffer, 7 );

        pass.uniform_desc_set
            = std::make_unique<engine::DescriptorSet>( std::move( uniform_desc_set ) );

        engine::DescriptorSet history_desc_set = engine::generate_descriptor_set(
            {
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLER,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            },
            VK_SHADER_STAGE_COMPUTE_BIT
        );

        engine::update_descriptor_set_rwimage(
            history_desc_set,
            output,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            0
        );
        engine::update_descriptor_set_rwimage(
            history_desc_set,
            history,
            VK_IMAGE_LAYOUT_GENERAL,
            4
        );

        pass.history_desc_set
            = std::make_unique<engine::DescriptorSet>( std::move( history_desc_set ) );
    }

    engine::add_cs_task(
        task_list,
        {
            .pipeline = engine::create_compute_pipeline(
                { pass.uniform_desc_set->layouts[0] },
                vk::create::shader_module( ANTI_ALIASING_SHADER_PATH ),
                "cs_main"
            ),
            .descriptor_sets = { pass.uniform_desc_set.get() },
            .group_size = glm::ivec3(
                ( engine.swapchain.extent.width + 7 ) / 8,
                ( engine.swapchain.extent.height + 7 ) / 8,
                1
            ),
        }
    );

    // cs_write_history samples output and stores to history.
    add_pipeline_barrier(
        task_list,
        PipelineBarrierDescriptor { .buffer_barriers = { },
                                    .image_barriers = {
                                        ImageBarrier {
                                            .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                            .src_access = VK_ACCESS_2_SHADER_WRITE_BIT,
                                            .src_layout = VK_IMAGE_LAYOUT_GENERAL,
                                            .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                            .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                                            .dst_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                            .image = &output,
                                            .range = VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
                                        },
                                        ImageBarrier {
                                            .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                            .src_access = VK_ACCESS_2_SHADER_READ_BIT,
                                            .src_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                            .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                            .dst_access = VK_ACCESS_2_SHADER_WRITE_BIT,
                                            .dst_layout = VK_IMAGE_LAYOUT_GENERAL,
                                            .image = &history,
                                            .range = VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
                                        },
                                    } }
    );

    engine::add_cs_task(
        task_list,
        {
            .pipeline = engine::create_compute_pipeline(
                { pass.history_desc_set->layouts[0] },
                vk::create::shader_module( HISTORY_SHADER_PATH ),
                "cs_write_history"
            ),
            .descriptor_sets = { pass.history_desc_set.get() },
            .group_size = glm::ivec3(
                ( engine.swapchain.extent.width + 7 ) / 8,
                ( engine.swapchain.extent.height + 7 ) / 8,
                1
            ),
        }
    );

    // Ensure write for the proper transition before... uh... the blit. Leave history readable
    add_pipeline_barrier(
        task_list,
        PipelineBarrierDescriptor { .buffer_barriers = { },
                                    .image_barriers = {
                                        ImageBarrier {
                                            .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                            .src_access = VK_ACCESS_2_SHADER_READ_BIT,
                                            .src_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                            .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                            .dst_access = VK_ACCESS_2_SHADER_WRITE_BIT,
                                            .dst_layout = VK_IMAGE_LAYOUT_GENERAL,
                                            .image = &output,
                                            .range = VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
                                        },
                                        ImageBarrier {
                                            .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                            .src_access = VK_ACCESS_2_SHADER_WRITE_BIT,
                                            .src_layout = VK_IMAGE_LAYOUT_GENERAL,
                                            .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                            .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                                            .dst_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                            .image = &history,
                                            .range = VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
                                        },
                                    } }
    );

    return pass;
}

void update_aa_uniform_buffer( engine::post::AAPass& aa_pass )
{
    const engine::State& engine = engine::State::GetConst();
    ub_data::AA aa_ub = aa_pass.buffer.get_data();
    aa_ub.mode = RuntimeSettings::GetValue( RacecarSettings::AA_MODE );
    aa_pass.buffer.set_data( aa_ub );
    aa_pass.buffer.update( engine.get_frame_index() );
}

}
