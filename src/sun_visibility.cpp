#include "sun_visibility.hpp"

#include "engine/descriptor_set.hpp"
#include "engine/pipeline.hpp"
#include "engine/pipeline_barrier.hpp"
#include "engine/rwimage.hpp"
#include "engine/ub_data.hpp"
#include "engine/uniform_buffer.hpp"
#include "terrain/terrain.hpp"
#include "vk/common.hpp"
#include "vk/create.hpp"

#include <vulkan/vulkan_core.h>

#include <string_view>

namespace racecar::atmosphere {

static constexpr std::string_view
    SUN_VISIBILITY_SHADER_PATH = "../shaders/sun_visibility/sun_visibility.spv";

void initialize_sun_visibility(
    SunVisibilityComputePass& sun_visibility,
    volumetric::Volumetric& volumetric,
    engine::DescriptorSet& gamestate_desc_set
)
{
    const engine::State& engine = engine::State::GetConst();

    sun_visibility.volumetric = &volumetric;
    sun_visibility.gamestate_desc_set = &gamestate_desc_set;

    sun_visibility.sun_visibility_shadow_map = engine::create_rwimage(
        VkExtent3D { .width = 512, .height = 512, .depth = 1 },
        VK_FORMAT_R16_SFLOAT,
        VK_IMAGE_TYPE_2D,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    );

    sun_visibility.terrain_size_data_uniform = create_uniform_buffer<ub_data::TerrainSizeData>(
        { },
        engine.frame_overlap
    );
    sun_visibility.terrain_size_data_uniform.set_data(
        ub_data::TerrainSizeData {
            .center = glm::vec4( 0.0f, 0.0f, 0.0f, 1.0f ),
            .size = glm::vec2( 2 * geometry::TERRAIN_HALF_WIDTH, 2 * geometry::TERRAIN_HALF_WIDTH ),
        }
    );
    sun_visibility.terrain_size_data_uniform.update_all();

    sun_visibility.visibility_write_desc_set = engine::generate_descriptor_set(
        {
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // sun_visibility
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // terrain_size_data
        },
        VK_SHADER_STAGE_COMPUTE_BIT
    );

    sun_visibility.visibility_read_desc_set = engine::generate_descriptor_set(
        {
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // terrain_size_data
            VK_DESCRIPTOR_TYPE_SAMPLER,
        },
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
    );

    engine::update_descriptor_set_rwimage(
        sun_visibility.visibility_write_desc_set,
        sun_visibility.sun_visibility_shadow_map,
        VK_IMAGE_LAYOUT_GENERAL,
        0
    );

    engine::update_descriptor_set_rwimage(
        sun_visibility.visibility_read_desc_set,
        sun_visibility.sun_visibility_shadow_map,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        0
    );

    engine::update_descriptor_set_uniform(
        sun_visibility.visibility_read_desc_set,
        sun_visibility.terrain_size_data_uniform,
        1
    );

    engine::update_descriptor_set_sampler(
        sun_visibility.visibility_read_desc_set,
        vk::Common::GetConst().global_samplers.linear_sampler,
        2
    );

    engine::update_descriptor_set_uniform(
        sun_visibility.visibility_write_desc_set,
        sun_visibility.terrain_size_data_uniform,
        1
    );

    sun_visibility.cs_sun_visibility_pipeline = engine::create_compute_pipeline(
        {
            volumetric.uniform_desc_set.layouts[0],
            volumetric.lut_desc_set.layouts[0],
            volumetric.sampler_desc_set.layouts[0],
            sun_visibility.visibility_write_desc_set.layouts[0],
            gamestate_desc_set.layouts[0],
        },
        vk::create::shader_module( SUN_VISIBILITY_SHADER_PATH ),
        "cs_sun_visibility"
    );
}

void compute_sun_visibility( SunVisibilityComputePass& sun_visibility, engine::TaskList& task_list )
{
    volumetric::Volumetric& volumetric = *sun_visibility.volumetric;

    engine::add_pipeline_barrier(
        task_list,
        engine::PipelineBarrierDescriptor {
            .buffer_barriers = { },
            .image_barriers = { engine::ImageBarrier {
                .src_stage = VK_PIPELINE_STAGE_2_NONE,
                .src_access = VK_ACCESS_2_NONE,
                .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dst_access = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dst_layout = VK_IMAGE_LAYOUT_GENERAL,
                .image = &sun_visibility.sun_visibility_shadow_map,
                .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR } },
            .label = "Sun Visibility Shadow Map Setup Transition Pipeline Barrier",
        }
    );

    engine::ComputeTask cs_sun_visibility_task = {
        sun_visibility.cs_sun_visibility_pipeline,
        {
            &volumetric.uniform_desc_set,
            &volumetric.lut_desc_set,
            &volumetric.sampler_desc_set,
            &sun_visibility.visibility_write_desc_set,
            sun_visibility.gamestate_desc_set,
        },
        glm::ivec3(
            sun_visibility.sun_visibility_shadow_map.images[0].image_extent.width / 8,
            sun_visibility.sun_visibility_shadow_map.images[0].image_extent.height / 8,
            1
        ),
    };

    engine::add_cs_task( task_list, cs_sun_visibility_task, "Sun Visibility" );

    engine::add_pipeline_barrier(
        task_list,
        engine::PipelineBarrierDescriptor {
            .buffer_barriers = { },
            .image_barriers = { engine::ImageBarrier {
                .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .src_access = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .src_layout = VK_IMAGE_LAYOUT_GENERAL,
                .dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
                    | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dst_access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dst_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .image = &sun_visibility.sun_visibility_shadow_map,
                .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR } },
            .label = "Sun Visibility Shadow Map Write to Read Pipeline Barrier",
        }
    );
}

}
