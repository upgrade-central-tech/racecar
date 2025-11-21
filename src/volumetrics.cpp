#include "volumetrics.hpp"
#include "engine/state.hpp"
#include "vk/common.hpp"
#include "vk/create.hpp"

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

    if ( !generate_noise( volumetric ) ) {
        return {};
    }

    volumetric.uniform_buffer = create_uniform_buffer<ub_data::Camera>(
        vulkan, {}, static_cast<size_t>( engine.frame_overlap ) );

    // Initialize descriptor sets
    volumetric.uniform_desc_set
        = engine::generate_descriptor_set( vulkan, engine, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT );

    volumetric.lut_desc_set
        = engine::generate_descriptor_set( vulkan, engine, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT );

    volumetric.sampler_desc_set
        = engine::generate_descriptor_set( vulkan, engine, { VK_DESCRIPTOR_TYPE_SAMPLER },
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT );

    engine::update_descriptor_set_uniform(
        vulkan, engine, volumetric.uniform_desc_set, volumetric.uniform_buffer, 0 );

    engine::update_descriptor_set_sampler(
        vulkan, engine, volumetric.sampler_desc_set, vulkan.global_samplers.linear_sampler, 0 );

    log::info( "[Engine] DESCRIPTOR SHOULD BE MADE BY THIS POINT.");

    // Create pipelines necesary for the volumetric to work laterr...
    return volumetric;
}

bool generate_noise( [[maybe_unused]] Volumetric& volumetric ) { return true; }

void draw_volumetric(
    [[maybe_unused]] Volumetric& volumetric, vk::Common& vulkan, const engine::State& engine, [[maybe_unused]] engine::TaskList& task_list )
{
    const geometry::scene::Mesh& volumetric_mesh = volumetric.scene_mesh;

    // We ideally don't want to render this at all resolution. That would take too much time!
    engine::GfxTask volumetric_gfx_task = {
        .render_target_is_swapchain = true,
        .extent = engine.swapchain.extent,
    };

    engine::Pipeline volumetric_pipeline;

    try {
        volumetric_pipeline = engine::create_gfx_pipeline( engine, vulkan,
            engine::get_vertex_input_state_create_info( volumetric_mesh ),
            {
                volumetric.uniform_desc_set.layouts[0],
                volumetric.lut_desc_set.layouts[0],
                volumetric.sampler_desc_set.layouts[0],
            },
            { engine.swapchain.image_format }, false,
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

    log::info( "[VOLUMETRIC] Volumetric gfx task added");
}

}
