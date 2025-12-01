#include "atmosphere_baker.hpp"

#include "engine/images.hpp"
#include "engine/pipeline.hpp"
#include "vk/create.hpp"
#include "vk/utility.hpp"

namespace racecar::atmosphere {

void prebake_octahedral_sky(
    const AtmosphereBaker& atms_baker, vk::Common& vulkan, engine::State& engine )
{
    engine::immediate_submit(
        vulkan, engine.immediate_submit, [&]( VkCommandBuffer command_buffer ) {
            bake_octahedral_sky_task( atms_baker, command_buffer );
        } );
}

void bake_octahedral_sky_task( const AtmosphereBaker& atms_baker, VkCommandBuffer command_buffer )
{
    const vk::mem::AllocatedImage& octahedral_sky = atms_baker.octahedral_sky;
    const engine::Pipeline& compute_pipeline = atms_baker.compute_pipeline;

    vk::utility::transition_image( command_buffer, octahedral_sky.image, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_NONE, VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT );

    vkCmdBindPipeline( command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline.handle );

    const Atmosphere& atms = *atms_baker.atmosphere;
    std::vector<VkDescriptorSet> bind_descs = { atms.uniform_desc_set.descriptor_sets[0],
        atms.lut_desc_set.descriptor_sets[0], atms.sampler_desc_set.descriptor_sets[0],
        atms_baker.octahedral_write.descriptor_sets[0] };

    vkCmdBindDescriptorSets( command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
        compute_pipeline.layout, 0, 4, bind_descs.data(), 0, nullptr );

    uint32_t x_groups = ( static_cast<uint32_t>( octahedral_sky.image_extent.width ) + 7 ) / 8;
    uint32_t y_groups = ( static_cast<uint32_t>( octahedral_sky.image_extent.width ) + 7 ) / 8;

    vkCmdDispatch( command_buffer, x_groups, y_groups, 1 );

    // vk::utility::transition_image( command_buffer, octahedral_sky.image, VK_IMAGE_LAYOUT_GENERAL,
    //     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT,
    //     VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT );
}

void initialize_atmosphere_baker(
    AtmosphereBaker& atms_baker, vk::Common& vulkan, engine::State& engine )
{
    uint32_t octahedral_sky_size = 1024;
    uint32_t irradiance_size = 32;

    atms_baker.octahedral_sky
        = engine::allocate_image( vulkan, { octahedral_sky_size, octahedral_sky_size, 1 },
            VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TYPE_2D, 1, 1, VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, false );

    atms_baker.octahedral_sky_irradiance = engine::create_rwimage( vulkan, engine,
        { irradiance_size, irradiance_size, 1 }, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TYPE_2D,
        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT );

    atms_baker.octahedral_write = engine::generate_descriptor_set( vulkan, engine,
        {
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        },
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT ),

    engine::update_descriptor_set_write_image(
        vulkan, engine, atms_baker.octahedral_write, atms_baker.octahedral_sky, 0 );

    engine::update_descriptor_set_rwimage( vulkan, engine, atms_baker.octahedral_write,
        atms_baker.octahedral_sky_irradiance, VK_IMAGE_LAYOUT_GENERAL, 1 );

    // Everything down here should be abstracted away.

    const Atmosphere& atms = *atms_baker.atmosphere;

    atms_baker.compute_pipeline = engine::create_compute_pipeline( vulkan,
        {
            atms.uniform_desc_set.layouts[0],
            atms.lut_desc_set.layouts[0],
            atms.sampler_desc_set.layouts[0],
            atms_baker.octahedral_write.layouts[0],
        },
        vk::create::shader_module( vulkan, "../shaders/atmosphere/bake_atmosphere.spv" ),
        "cs_bake_atmosphere" );

    prebake_octahedral_sky( atms_baker, vulkan, engine );
}

void compute_octahedral_sky_irradiance( AtmosphereBaker& atms_baker, vk::Common& vulkan,
    [[maybe_unused]] engine::State& engine, [[maybe_unused]] engine::TaskList& task_list )
{
    Atmosphere& atms = *atms_baker.atmosphere;

    engine::Pipeline cs_sky_irradiance_pipeline = engine::create_compute_pipeline( vulkan,
        {
            atms.uniform_desc_set.layouts[0],
            atms.lut_desc_set.layouts[0],
            atms.sampler_desc_set.layouts[0],
            atms_baker.octahedral_write.layouts[0],
        },
        vk::create::shader_module( vulkan, "../shaders/atmosphere/bake_atmosphere_irradiance.spv" ),
        "cs_bake_atmosphere_irradiance" );

    uint32_t x_groups = ( static_cast<uint32_t>(
                              atms_baker.octahedral_sky_irradiance.images[0].image_extent.width )
                            + 7 )
        / 8;
    uint32_t y_groups = ( static_cast<uint32_t>(
                              atms_baker.octahedral_sky_irradiance.images[0].image_extent.width )
                            + 7 )
        / 8;

    engine::ComputeTask cs_sky_irradiance_task = {
        cs_sky_irradiance_pipeline,
        {
            &atms.uniform_desc_set,
            &atms.lut_desc_set,
            &atms.sampler_desc_set,
            &atms_baker.octahedral_write,
        },
        glm::ivec3( x_groups, y_groups, 1 ),
    };

    engine::add_pipeline_barrier( task_list,
        engine::PipelineBarrierDescriptor { .buffer_barriers = {},
            .image_barriers
            = { engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                .src_access = VK_ACCESS_2_NONE,
                .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dst_access = VK_ACCESS_SHADER_WRITE_BIT,
                .dst_layout = VK_IMAGE_LAYOUT_GENERAL,
                .image = atms_baker.octahedral_sky_irradiance,
                .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR } } } );

    engine::add_cs_task( task_list, cs_sky_irradiance_task );

    // Read-only from this point on.
    engine::add_pipeline_barrier( task_list,
        engine::PipelineBarrierDescriptor { .buffer_barriers = {},
            .image_barriers
            = { engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .src_access = VK_ACCESS_SHADER_WRITE_BIT,
                .src_layout = VK_IMAGE_LAYOUT_GENERAL,
                .dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                .dst_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .image = atms_baker.octahedral_sky_irradiance,
                .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR } } } );
};

void compute_octahedral_sky_mips( AtmosphereBaker& atms_baker, vk::Common& vulkan,
    engine::State& engine, engine::TaskList& task_list )
{
    uint32_t mip0_size = 512;
    uint32_t mip_levels = 5;

    atms_baker.octahedral_sky_test
        = engine::create_rwimage_mips( vulkan, engine, { mip0_size, mip0_size, 1 },
            VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TYPE_2D, VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, mip_levels );

    Atmosphere& atms = *atms_baker.atmosphere;

    for ( size_t mip = 0; mip < mip_levels; mip++ ) {
        atms_baker.octahedral_mip_writes.push_back( engine::generate_descriptor_set( vulkan, engine,
            {
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            },
            VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT ) );

        engine::update_descriptor_set_write_image(
            vulkan, engine, atms_baker.octahedral_mip_writes[mip], atms_baker.octahedral_sky, 0 );
        engine::update_descriptor_set_rwimage_mip( vulkan, engine,
            atms_baker.octahedral_mip_writes[mip], atms_baker.octahedral_sky_test,
            VK_IMAGE_LAYOUT_GENERAL, 2, mip );

        UniformBuffer mip_data
            = create_uniform_buffer<ub_data::OctahedralData>( vulkan, {}, engine.frame_overlap );
        float roughness = (float)mip / (float)( mip_levels - 1 );
        mip_data.set_data( { glm::vec4( mip, roughness, 0.0f, 0.0f ) } );

        // Set this only once per frame.
        for ( uint32_t frame_index = 0; frame_index < engine.frame_overlap; frame_index++ ) {
            mip_data.update( vulkan, frame_index );
        }

        engine::update_descriptor_set_uniform(
            vulkan, engine, atms_baker.octahedral_mip_writes[mip], mip_data, 3 );
    }

    engine::Pipeline cs_octahedral_mip_pipeline = engine::create_compute_pipeline( vulkan,
        {
            atms.uniform_desc_set.layouts[0],
            atms.lut_desc_set.layouts[0],
            atms.sampler_desc_set.layouts[0],
            atms_baker.octahedral_write.layouts[0],
        },
        vk::create::shader_module( vulkan, "../shaders/atmosphere/bake_atmosphere_mips.spv" ),
        "cs_bake_atmosphere_mips" );

    engine::add_pipeline_barrier( task_list,
        engine::PipelineBarrierDescriptor { .buffer_barriers = {},
            .image_barriers
            = { engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                .src_access = VK_ACCESS_2_NONE,
                .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dst_access = VK_ACCESS_SHADER_WRITE_BIT,
                .dst_layout = VK_IMAGE_LAYOUT_GENERAL,
                .image = atms_baker.octahedral_sky_test,
                .range = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = mip_levels,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                } } } } );

    for ( size_t mip = 0; mip < mip_levels; mip++ ) {
        // Swap the descriptor set to write to each mip level...
        // Need to change what mip level we're writing to
        uint32_t mip_size = mip0_size >> mip;

        glm::ivec2 dims = {
            ( mip_size + 7 ) / 8,
            ( mip_size + 7 ) / 8,
        };

        engine::ComputeTask cs_mip_task = { cs_octahedral_mip_pipeline,
            {
                &atms.uniform_desc_set,
                &atms.lut_desc_set,
                &atms.sampler_desc_set,
                &atms_baker.octahedral_mip_writes[mip],
            },
            glm::ivec3( dims, 1 ) };

        engine::add_cs_task( task_list, cs_mip_task );
    }

    engine::add_pipeline_barrier( task_list,
        engine::PipelineBarrierDescriptor { .buffer_barriers = {},
            .image_barriers
            = { engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .src_access = VK_ACCESS_SHADER_WRITE_BIT,
                .src_layout = VK_IMAGE_LAYOUT_GENERAL,
                .dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                .dst_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .image = atms_baker.octahedral_sky_test,
                .range = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = mip_levels,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                } } } } );
}

}
