#include "ao.hpp"

#include "../../gui.hpp"
#include "../../vk/create.hpp"
#include "engine/descriptor_set.hpp"
#include "engine/pipeline_barrier.hpp"
#include "engine/rwimage.hpp"
#include "engine/task_list.hpp"

#include <vulkan/vulkan_core.h>

const std::filesystem::path AO_SHADER_MODULE_PATH = "../shaders/post/ao/ao.spv";

namespace racecar::engine::post {

void initialize_ao_pass( AoPass& ao_pass )
{
    const engine::State& engine = engine::State::GetConst();
    ao_pass.ao_buffer = create_uniform_buffer<ub_data::AOData>(
        { },
        static_cast<size_t>( engine.frame_overlap )
    );

    ao_pass.uniform_desc_set = engine::generate_descriptor_set(
        {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        },
        VK_SHADER_STAGE_COMPUTE_BIT
    );

    ao_pass.texture_desc_set = engine::generate_descriptor_set(
        {
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        },
        VK_SHADER_STAGE_COMPUTE_BIT
    );

    engine::update_descriptor_set_uniform( ao_pass.uniform_desc_set, *ao_pass.camera_buffer, 0 );
    engine::update_descriptor_set_uniform( ao_pass.uniform_desc_set, ao_pass.ao_buffer, 1 );

    ao_pass.ao_output = engine::create_rwimage(
        VkExtent3D( engine.swapchain.extent.width, engine.swapchain.extent.height, 1 ),
        VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT,
        VkImageType::VK_IMAGE_TYPE_2D,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
    );

    engine::update_descriptor_set_rwimage(
        ao_pass.texture_desc_set,
        ao_pass.ao_output,
        VK_IMAGE_LAYOUT_GENERAL,
        0
    );

    engine::update_descriptor_set_rwimage(
        ao_pass.texture_desc_set,
        *ao_pass.GBuffer_Normal,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        1
    );
    engine::update_descriptor_set_depth_image(
        ao_pass.texture_desc_set,
        *ao_pass.GBuffer_Depth,
        2
    );

    // for reading AO as a texture
    ao_pass.ao_read_descriptor_set = engine::generate_descriptor_set(
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
        VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    );

    engine::update_descriptor_set_rwimage(
        ao_pass.ao_read_descriptor_set,
        ao_pass.ao_output,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        0
    );

    return;
}

void add_ao( AoPass& ao_pass, TaskList& task_list )
{
    engine::Pipeline cs_ao_pipeline = engine::create_compute_pipeline(
        {
            ao_pass.uniform_desc_set.layouts[0],
            ao_pass.texture_desc_set.layouts[0],
        },
        vk::create::shader_module( AO_SHADER_MODULE_PATH ),
        "cs_ao"
    );

    size_t dim_x = ( ao_pass.ao_output.images[0].image_extent.width + 7 ) / 8;
    size_t dim_y = ( ao_pass.ao_output.images[0].image_extent.height + 7 ) / 8;

    // pre-compute task transition ao_output to general
    engine::add_pipeline_barrier(
        task_list,
        { .buffer_barriers = { },
          .image_barriers
          = { engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                     .src_access = VK_ACCESS_2_NONE,
                                     .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                                     .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                     .dst_access = VK_ACCESS_2_SHADER_WRITE_BIT,
                                     .dst_layout = VK_IMAGE_LAYOUT_GENERAL,
                                     .image = &ao_pass.ao_output,
                                     .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR } },
          .label = "AO Output Pre-Pass Pipeline Barrier" }
    );

    engine::ComputeTask cs_ao_task = {
        cs_ao_pipeline,
        {
            &ao_pass.uniform_desc_set,
            &ao_pass.texture_desc_set,
        },
        glm::ivec3( dim_x, dim_y, 1 ),
    };

    engine::add_cs_task( task_list, cs_ao_task, "AO" );

    // post-compute task transition ao_output to shader read only
    engine::add_pipeline_barrier(
        task_list,
        { .buffer_barriers = { },
          .image_barriers
          = { engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                     .src_access = VK_ACCESS_2_SHADER_WRITE_BIT,
                                     .src_layout = VK_IMAGE_LAYOUT_GENERAL,
                                     .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
                                         | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                     .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                                     .dst_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                     .image = &ao_pass.ao_output,
                                     .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR } },
          .label = "AO Output Post-Pass Pipeline Barrier" }
    );

    return;
}

void update_ao_uniform_buffer( const gui::Gui& gui, engine::post::AoPass& ao_pass )
{
    const engine::State& engine = engine::State::GetConst();
    ub_data::AOData ao_ub = ao_pass.ao_buffer.get_data();

    ao_ub.packed_floats0 = glm::vec4(
        gui.ao.thickness,
        gui.ao.radius,
        gui.ao.offset,
        gui.ao.enable_debug ? 1.0f : 0.0f
    );
    ao_ub.packed_floats1 = glm::vec4( gui.ao.enable_ao ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f );

    ao_pass.ao_buffer.set_data( ao_ub );
    ao_pass.ao_buffer.update( engine.get_frame_index() );
}

}
