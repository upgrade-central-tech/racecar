#include "ao.hpp"

#include "../../vk/create.hpp"

const std::filesystem::path AO_SHADER_MODULE_PATH = "../shaders/post/ao/ao.spv";

namespace racecar::engine::post {

void initialize_ao_pass( vk::Common& vulkan, engine::State& engine, AoPass& ao_pass )
{
    ao_pass.uniform_desc_set = engine::generate_descriptor_set( vulkan, engine,
        {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        },
        VK_SHADER_STAGE_COMPUTE_BIT );

    ao_pass.texture_desc_set = engine::generate_descriptor_set( vulkan, engine,
        {
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        },
        VK_SHADER_STAGE_COMPUTE_BIT );

    engine::update_descriptor_set_uniform(
        vulkan, engine, ao_pass.uniform_desc_set, *ao_pass.camera_buffer, 0 );
    engine::update_descriptor_set_rwimage(
        vulkan, engine, ao_pass.texture_desc_set, *ao_pass.out_color, VK_IMAGE_LAYOUT_GENERAL, 0 );

    engine::update_descriptor_set_rwimage( vulkan, engine, ao_pass.texture_desc_set,
        *ao_pass.in_color, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1 );
    engine::update_descriptor_set_rwimage( vulkan, engine, ao_pass.texture_desc_set,
        *ao_pass.GBuffer_Normal, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 2 );
    engine::update_descriptor_set_rwimage( vulkan, engine, ao_pass.texture_desc_set,
        *ao_pass.GBuffer_Depth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 3 );

    return;
}

void add_ao( AoPass& ao_pass, vk::Common& vulkan, engine::State& engine, TaskList& task_list )
{
    initialize_ao_pass( vulkan, engine, ao_pass );

    engine::Pipeline cs_ao_pipeline = engine::create_compute_pipeline( vulkan,
        {
            ao_pass.uniform_desc_set.layouts[0],
            ao_pass.texture_desc_set.layouts[0],
        },
        vk::create::shader_module( vulkan, AO_SHADER_MODULE_PATH ), "cs_ao" );

    size_t dim_x = ( ao_pass.out_color->images[0].image_extent.width + 7 ) / 8;
    size_t dim_y = ( ao_pass.out_color->images[0].image_extent.height + 7 ) / 8;

    engine::ComputeTask cs_ao_task = {
        cs_ao_pipeline,
        {
            &ao_pass.uniform_desc_set,
            &ao_pass.texture_desc_set,
        },
        glm::ivec3( dim_x, dim_y, 1 ),
    };

    engine::add_cs_task( task_list, cs_ao_task );

    return;
}

}
