#include "volumetrics.hpp"

#include "engine/descriptor_set.hpp"
#include "engine/images.hpp"
#include "engine/state.hpp"
#include "vk/common.hpp"
#include "vk/create.hpp"
#include "vk/utility.hpp"

#include <string_view>

namespace racecar::volumetric {

constexpr std::string_view VOLUMETRIC_FILE_PATH = "../assets/cube.glb";
[[maybe_unused]] constexpr std::string_view VOLUMETRIC_SHADER_MODULE_PATH
    = "../shaders/cloud_debug/cloud_debug.spv";

Volumetric initialize( vk::Common& vulkan, engine::State& engine )
{
    Volumetric volumetric;
    scene::Scene& scene = volumetric.scene;
    geometry::scene::Mesh& scene_mesh = volumetric.scene_mesh;

    // Generate preliminary data for the volumetrics to work
    scene::load_gltf(
        vulkan, engine, VOLUMETRIC_FILE_PATH, scene, scene_mesh.vertices, scene_mesh.indices );

    scene_mesh.mesh_buffers
        = geometry::scene::upload_mesh( vulkan, engine, scene_mesh.indices, scene_mesh.vertices );

    if ( !generate_noise( volumetric, vulkan, engine ) ) {
        return {};
    }

    volumetric.uniform_buffer = create_uniform_buffer<ub_data::Camera>(
        vulkan, {}, static_cast<size_t>( engine.frame_overlap ) );

    // Initialize descriptor sets
    volumetric.uniform_desc_set = engine::generate_descriptor_set( vulkan, engine,
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT );

    volumetric.lut_desc_set = engine::generate_descriptor_set( vulkan, engine,
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT );

    volumetric.sampler_desc_set = engine::generate_descriptor_set( vulkan, engine,
        { VK_DESCRIPTOR_TYPE_SAMPLER },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT );

    engine::update_descriptor_set_uniform(
        vulkan, engine, volumetric.uniform_desc_set, volumetric.uniform_buffer, 0 );

    engine::update_descriptor_set_sampler(
        vulkan, engine, volumetric.sampler_desc_set, vulkan.global_samplers.linear_sampler, 0 );

    log::info( "[Engine] DESCRIPTOR SHOULD BE MADE BY THIS POINT." );

    // Create pipelines necesary for the volumetric to work laterr...
    return volumetric;
}

bool generate_noise(
    [[maybe_unused]] Volumetric& volumetric, vk::Common& vulkan, engine::State& engine )
{
    // Cumulus map generation
    {
        uint32_t cumulus_map_size = 256;

        volumetric.cumulus_map = engine::allocate_image( vulkan,
            { cumulus_map_size, cumulus_map_size, 1 }, VK_FORMAT_R8_UNORM, VK_IMAGE_TYPE_2D, 1, 1,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, false );

        engine::DescriptorSet cumulus_desc_set = engine::generate_descriptor_set( vulkan, engine,
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
            VK_SHADER_STAGE_COMPUTE_BIT );

        engine::update_descriptor_set_write_image(
            vulkan, engine, cumulus_desc_set, volumetric.cumulus_map, 2 );

        VkShaderModule generate_cumulus_module
            = vk::create::shader_module( vulkan, "../shaders/cloud_debug/cs_generate_cumulus.spv" );

        engine::Pipeline compute_pipeline = engine::create_compute_pipeline( vulkan,
            { cumulus_desc_set.layouts[0] }, generate_cumulus_module, "cs_generate_cumulus" );

        log::info( "[VOLUMETRIC] Compute pipeline for cumulus made" );

        engine::immediate_submit(
            vulkan, engine.immediate_submit, [&]( VkCommandBuffer command_buffer ) {
                vk::utility::transition_image( command_buffer, volumetric.cumulus_map.image,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_NONE,
                    VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT );

                vkCmdBindPipeline(
                    command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline.handle );

                vkCmdBindDescriptorSets( command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    compute_pipeline.layout, 0, 1, cumulus_desc_set.descriptor_sets.data(), 0,
                    nullptr );

                uint32_t x_groups = ( cumulus_map_size + 7 ) / 8;
                uint32_t y_groups = ( cumulus_map_size + 7 ) / 8;

                vkCmdDispatch( command_buffer, x_groups, y_groups, 1 );

                vk::utility::transition_image( command_buffer, volumetric.cumulus_map.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT );
            } );

        log::info( "[VOLUMETRIC] Ran awesome submit for the cumulus noise map generation." );
    }

    // Low frequency noise map generation
    {
        uint32_t low_freq_noise_size = 128;

        volumetric.low_freq_noise = engine::allocate_image( vulkan,
            { low_freq_noise_size, low_freq_noise_size, low_freq_noise_size }, VK_FORMAT_R8_UNORM,
            VK_IMAGE_TYPE_3D, 1, 1, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            false );

        engine::DescriptorSet low_freq_desc_set = engine::generate_descriptor_set(
            vulkan, engine, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE }, VK_SHADER_STAGE_COMPUTE_BIT );

        engine::update_descriptor_set_write_image(
            vulkan, engine, low_freq_desc_set, volumetric.low_freq_noise, 0 );

        VkShaderModule generate_low_freq_module = vk::create::shader_module(
            vulkan, "../shaders/cloud_debug/cs_generate_low_frequency.spv" );

        engine::Pipeline compute_pipeline
            = engine::create_compute_pipeline( vulkan, { low_freq_desc_set.layouts[0] },
                generate_low_freq_module, "cs_generate_low_frequency" );

        log::info( "[VOLUMETRIC] Compute pipeline for low frequency noise made" );

        engine::immediate_submit(
            vulkan, engine.immediate_submit, [&]( VkCommandBuffer command_buffer ) {
                vk::utility::transition_image( command_buffer, volumetric.low_freq_noise.image,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_NONE,
                    VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT );

                vkCmdBindPipeline(
                    command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline.handle );

                vkCmdBindDescriptorSets( command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    compute_pipeline.layout, 0, 1, low_freq_desc_set.descriptor_sets.data(), 0,
                    nullptr );

                uint32_t x_groups = ( low_freq_noise_size + 7 ) / 8;
                uint32_t y_groups = ( low_freq_noise_size + 7 ) / 8;
                uint32_t z_groups = ( low_freq_noise_size + 7 ) / 8;

                vkCmdDispatch( command_buffer, x_groups, y_groups, z_groups );

                vk::utility::transition_image( command_buffer, volumetric.low_freq_noise.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                    VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT );
            } );

        log::info( "[VOLUMETRIC] Ran submit for low frequency noise map generation" );
    }

    // high frequency noise map generation
    {
        uint32_t high_freq_noise_size = 32;

        volumetric.high_freq_noise = engine::allocate_image( vulkan,
            { high_freq_noise_size, high_freq_noise_size, high_freq_noise_size },
            VK_FORMAT_R8_UNORM, VK_IMAGE_TYPE_3D, 1, 1,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, false );

        engine::DescriptorSet high_freq_desc_set = engine::generate_descriptor_set( vulkan, engine,
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
            VK_SHADER_STAGE_COMPUTE_BIT );

        engine::update_descriptor_set_write_image(
            vulkan, engine, high_freq_desc_set, volumetric.high_freq_noise, 1 );

        VkShaderModule generate_high_freq_module = vk::create::shader_module(
            vulkan, "../shaders/cloud_debug/cs_generate_high_frequency.spv" );

        engine::Pipeline compute_pipeline
            = engine::create_compute_pipeline( vulkan, { high_freq_desc_set.layouts[0] },
                generate_high_freq_module, "cs_generate_high_frequency" );

        log::info( "[VOLUMETRIC] Compute pipeline for high frequency noise made" );

        engine::immediate_submit(
            vulkan, engine.immediate_submit, [&]( VkCommandBuffer command_buffer ) {
                vk::utility::transition_image( command_buffer, volumetric.high_freq_noise.image,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_NONE,
                    VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT );

                vkCmdBindPipeline(
                    command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline.handle );

                vkCmdBindDescriptorSets( command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    compute_pipeline.layout, 0, 1, high_freq_desc_set.descriptor_sets.data(), 0,
                    nullptr );

                uint32_t x_groups = ( high_freq_noise_size + 7 ) / 8;
                uint32_t y_groups = ( high_freq_noise_size + 7 ) / 8;
                uint32_t z_groups = ( high_freq_noise_size + 7 ) / 8;

                vkCmdDispatch( command_buffer, x_groups, y_groups, z_groups );

                vk::utility::transition_image( command_buffer, volumetric.high_freq_noise.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                    VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT );
            } );

        log::info( "[VOLUMETRIC] Ran submit for high frequency noise map generation" );
    }

    return true;
}

void draw_volumetric( [[maybe_unused]] Volumetric& volumetric, vk::Common& vulkan,
    engine::State& engine, [[maybe_unused]] engine::TaskList& task_list )
{
    const geometry::scene::Mesh& volumetric_mesh = volumetric.scene_mesh;

    // We ideally don't want to render this at all resolution. That would take too much time!
    engine::GfxTask volumetric_gfx_task = {
        .render_target_is_swapchain = true,
        .extent = engine.swapchain.extent,
    };

    engine::Pipeline volumetric_pipeline;

    engine::update_descriptor_set_image(
        vulkan, engine, volumetric.lut_desc_set, volumetric.low_freq_noise, 0 );
    engine::update_descriptor_set_image(
        vulkan, engine, volumetric.lut_desc_set, volumetric.high_freq_noise, 1 );
    engine::update_descriptor_set_image(
        vulkan, engine, volumetric.lut_desc_set, volumetric.cumulus_map, 2 );

    try {
        volumetric_pipeline = engine::create_gfx_pipeline( engine, vulkan,
            engine::get_vertex_input_state_create_info( volumetric_mesh ),
            {
                volumetric.uniform_desc_set.layouts[0],
                volumetric.lut_desc_set.layouts[0],
                volumetric.sampler_desc_set.layouts[0],
            },
            { engine.swapchain.image_format }, VK_SAMPLE_COUNT_1_BIT, false,
            vk::create::shader_module( vulkan, VOLUMETRIC_SHADER_MODULE_PATH ) );
    } catch ( const Exception& ex ) {
        log::error( "Failed to create volumetrics graphics pipeline: {}", ex.what() );
        throw;
    }

    volumetric_gfx_task.draw_tasks.push_back( {
            .draw_resource_descriptor = {
                    .vertex_buffers = { volumetric_mesh.mesh_buffers.vertex_buffer.handle },
                    .index_buffer = volumetric_mesh.mesh_buffers.index_buffer.handle,
                    .vertex_buffer_offsets = { 0 },
                    .index_count = static_cast<uint32_t>( volumetric_mesh.indices.size() ),
            },
            .descriptor_sets = {
                &volumetric.uniform_desc_set,
                &volumetric.lut_desc_set,
                &volumetric.sampler_desc_set,
            },
            .pipeline = volumetric_pipeline,
        } );

    engine::add_gfx_task( task_list, volumetric_gfx_task );

    log::info( "[VOLUMETRIC] Volumetric gfx task added" );
}

}
