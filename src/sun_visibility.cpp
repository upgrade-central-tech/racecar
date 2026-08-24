#include "sun_visibility.hpp"

#include "engine/pipeline.hpp"
#include "vk/create.hpp"

#include <string_view>

namespace racecar::atmosphere {

static constexpr std::string_view SUN_VISIBILITY_SHADER_PATH
    = "../shaders/sun_visibility/sun_visibility.spv";

void initialize_sun_visibility(
    SunVisibilityComputePass& sun_visibility,
    volumetric::Volumetric& volumetric,
    engine::DescriptorSet& gamestate_desc_set
)
{
    const engine::State& engine = engine::State::GetConst();

    sun_visibility.volumetric = &volumetric;
    sun_visibility.gamestate_desc_set = &gamestate_desc_set;

    sun_visibility.visibility_buffer.clear();
    sun_visibility.visibility_buffer.reserve( engine.frame_overlap );

    for ( uint32_t i = 0; i < engine.frame_overlap; ++i ) {
        sun_visibility.visibility_buffer.push_back(
            vk::mem::create_buffer(
                sizeof( float ),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VMA_MEMORY_USAGE_GPU_ONLY
            )
        );
    }

    sun_visibility.visibility_desc_set = engine::generate_descriptor_set(
        {
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // sun_visibility
        },
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
    );

    engine::update_descriptor_set_storage_buffer_per_frame(
        sun_visibility.visibility_desc_set,
        sun_visibility.visibility_buffer,
        0
    );

    sun_visibility.cs_sun_visibility_pipeline = engine::create_compute_pipeline(
        {
            volumetric.uniform_desc_set.layouts[0],
            volumetric.lut_desc_set.layouts[0],
            volumetric.sampler_desc_set.layouts[0],
            sun_visibility.visibility_desc_set.layouts[0],
            gamestate_desc_set.layouts[0],
        },
        vk::create::shader_module( SUN_VISIBILITY_SHADER_PATH ),
        "cs_sun_visibility"
    );
}

void compute_sun_visibility(
    SunVisibilityComputePass& sun_visibility, engine::TaskList& task_list
)
{
    volumetric::Volumetric& volumetric = *sun_visibility.volumetric;

    engine::ComputeTask cs_sun_visibility_task = {
        sun_visibility.cs_sun_visibility_pipeline,
        {
            &volumetric.uniform_desc_set,
            &volumetric.lut_desc_set,
            &volumetric.sampler_desc_set,
            &sun_visibility.visibility_desc_set,
            sun_visibility.gamestate_desc_set,
        },
        glm::ivec3( 1, 1, 1 ),
    };

    engine::add_cs_task( task_list, cs_sun_visibility_task );

    engine::add_pipeline_barrier(
        task_list,
        engine::PipelineBarrierDescriptor {
            .buffer_barriers
            = { engine::BufferBarrier { .buffer = sun_visibility.visibility_buffer,
                                        .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                        .src_access = VK_ACCESS_2_SHADER_WRITE_BIT,
                                        .dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
                                            | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                        .dst_access = VK_ACCESS_2_SHADER_READ_BIT } },
            .image_barriers = { },
        }
    );
}

}
