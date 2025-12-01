#include "terrain.hpp"

#include "../engine/gfx_task.hpp"
#include "../engine/images.hpp"
#include "../engine/task_list.hpp"
#include "../geometry/gpu_mesh_buffers.hpp"
#include "../vk/create.hpp"

const std::filesystem::path TERRAIN_SHADER_PREPASS_MODULE_PATH
    = "../shaders/deferred/terrain_prepass.spv";
const std::filesystem::path TERRAIN_SHADER_LIGHTING_MODULE_PATH
    = "../shaders/terrain/cs_terrain_draw.spv";

// TEST FILE PATHS...
const std::filesystem::path TEST_LAYER_MASK_PATH = "../assets/LUT/test_terrain_map.bmp";
const std::filesystem::path TEST_GRASS_ALBEDO_ROUGHNESS_PATH
    = "../assets/terrain/better_grass/grass_albedo_roughness.png";
const std::filesystem::path TEST_GRASS_NORMAL_AO_PATH
    = "../assets/terrain/better_grass/grass_normal_ao.png";


const float TERRAIN_TILE_WIDTH = 10.0f;
const size_t TERRAIN_NUM_TILES = 50;

namespace racecar::geometry {

void initialize_terrain( vk::Common& vulkan, engine::State& engine, Terrain& terrain )
{
    // Generate enough information for just one planar quad. Expand it later on arbitrarily
    [[maybe_unused]] int32_t size = 1;

    // float scale = 10.0f;
    float offset_y = -0.15f;

    terrain.vertices.clear();
    terrain.indices.clear();

    const float half_width = (TERRAIN_NUM_TILES * TERRAIN_TILE_WIDTH) / 2.0f;

    // Generate vertices
    for (size_t z = 0; z <= TERRAIN_NUM_TILES; ++z) {
        for (size_t x = 0; x <= TERRAIN_NUM_TILES; ++x) {
            float xpos = float(x) * TERRAIN_TILE_WIDTH - half_width;
            float zpos = float(z) * TERRAIN_TILE_WIDTH - half_width;
            terrain.vertices.push_back({ glm::vec3(xpos, offset_y, zpos), glm::vec3(0,1,0) });
        }
    }

    // Generate indices
    for (size_t z = 0; z < TERRAIN_NUM_TILES; ++z) {
        for (size_t x = 0; x < TERRAIN_NUM_TILES; ++x) {
            unsigned int top_left = (unsigned int)(z * (TERRAIN_NUM_TILES + 1) + x);
            unsigned int top_right = (unsigned int)(top_left + 1);
            unsigned int bottom_left = (unsigned int)((z + 1) * (TERRAIN_NUM_TILES + 1) + x);
            unsigned int bottom_right = (unsigned int)(bottom_left + 1);

            terrain.indices.push_back(top_left);     // [0] TL
            terrain.indices.push_back(top_right);    // [1] TR
            terrain.indices.push_back(bottom_left);  // [2] BL
            terrain.indices.push_back(bottom_right); // [3] BR
        }
    }

    // Create respective mesh buffers using CPU data
    geometry::create_mesh_buffers( vulkan, terrain.mesh_buffers,
        sizeof( TerrainVertex ) * terrain.vertices.size(),
        sizeof( int32_t ) * terrain.indices.size() );

    // Upload to GPU
    geometry::upload_mesh_buffers(
        vulkan, engine, terrain.mesh_buffers, terrain.vertices.data(), terrain.indices.data() );

    // Build descriptors
    terrain.prepass_uniform_desc_set = engine::generate_descriptor_set( vulkan, engine,
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT );

    terrain.test_layer_mask
        = engine::load_image( TEST_LAYER_MASK_PATH, vulkan, engine, 2, VK_FORMAT_R8G8_UNORM );

    terrain.grass_albedo_roughness = engine::load_image(
        TEST_GRASS_ALBEDO_ROUGHNESS_PATH, vulkan, engine, 4, VK_FORMAT_R8G8B8A8_UNORM );

    terrain.grass_normal_ao = engine::load_image(
        TEST_GRASS_NORMAL_AO_PATH, vulkan, engine, 4, VK_FORMAT_R8G8B8A8_UNORM );
}

void draw_terrain_prepass( Terrain& terrain, vk::Common& vulkan, engine::State& engine,
    const engine::RWImage& GBuffer_Position, const engine::RWImage& GBuffer_Normal,
    const engine::RWImage& depth_image, UniformBuffer<ub_data::Camera>& camera_buffer,
    [[maybe_unused]] engine::DepthPrepassMS& depth_prepass_ms_task, engine::TaskList& task_list )
{
    // The deferred set-up makes this really complicated, because this means that
    // the rendering for this can't be done in its own shader.
    //
    // https://advances.realtimerendering.com/s2023/Etienne(ATVI)-Large%20Scale%20Terrain%20Rendering%20with%20notes%20(Advances%202023).pdf
    // Slide 24 notes that terrain writes to the depth buffer
    // Geometric normal is also written to a g-buffer
    // We write to the GBuffer_Position, but we need to specifically modify the stencil bit s.t it
    // it has ID 2 for terrain. In the opaque pass, "terrain is deferred rendered"

    // For now, since we don't have a stencil setup, I think it's possible to use the alpha channel
    // in the position GBuffer to store an ID, s.t 0 is nothing, 1 is car shading, and 2 is terrain.
    // Writes to the GBuffer depth and GBuffer normals for now

    engine::update_descriptor_set_uniform(
        vulkan, engine, terrain.prepass_uniform_desc_set, camera_buffer, 0 );

    terrain.terrain_prepass_task = {
        .render_target_is_swapchain = false,
        .color_attachments = { GBuffer_Position, GBuffer_Normal },
        .depth_image = { depth_image },
        .extent = engine.swapchain.extent,
    };

    engine::Pipeline terrain_prepass_pipeline;
    try {
        terrain_prepass_pipeline = engine::create_gfx_pipeline( engine, vulkan,
            engine::get_vertex_input_state_create_info( terrain ),
            { terrain.prepass_uniform_desc_set.layouts[0] },
            { VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT }, VK_SAMPLE_COUNT_1_BIT,
            false, true, vk::create::shader_module( vulkan, TERRAIN_SHADER_PREPASS_MODULE_PATH ), false);
    } catch ( const Exception& ex ) {
        log::error( "Failed to create terrain prepass graphics pipeline: {}", ex.what() );
        throw;
    }

    log::info( "[Terrain] In the process of doing something funny. Passed pipeline point" );

    engine::DrawResourceDescriptor draw_descriptor = {
        .vertex_buffers = { terrain.mesh_buffers.vertex_buffer.handle },
        .index_buffer = terrain.mesh_buffers.index_buffer.handle,
        .vertex_buffer_offsets = { 0 },
        .index_count = static_cast<uint32_t>( terrain.indices.size() ),
    };

    terrain.terrain_prepass_task.draw_tasks.push_back( {
        .draw_resource_descriptor = draw_descriptor,
        .descriptor_sets = {
            &terrain.prepass_uniform_desc_set,
        },
        .pipeline = terrain_prepass_pipeline,
    } );

    engine::add_gfx_task( task_list, terrain.terrain_prepass_task );
    // PushDepthPrepassMS( depth_prepass_ms_task, draw_descriptor );
}

void draw_terrain( Terrain& terrain, vk::Common& vulkan, engine::State& engine,
    engine::TaskList& task_list, TerrainLightingInfo& info )
{
    // Can we assume the color_attachment, by this point, is in a write-only state?

    terrain.uniform_desc_set = engine::generate_descriptor_set( vulkan, engine,
        {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        },
        VK_SHADER_STAGE_COMPUTE_BIT );

    terrain.texture_desc_set = engine::generate_descriptor_set( vulkan, engine,
        {
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // GBuffer Position
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // GBuffer Normal
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // Test layer mask (our fake OMPV solution)
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // Grass albedo
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // Grass normal + AO packed
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        },
        VK_SHADER_STAGE_COMPUTE_BIT );

    terrain.lut_desc_set = engine::generate_descriptor_set( vulkan, engine,
        {
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // Octahedral sky
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // Octahedral irradiance
        },
        VK_SHADER_STAGE_COMPUTE_BIT );

    terrain.sampler_desc_set = engine::generate_descriptor_set( vulkan, engine,
        {
            VK_DESCRIPTOR_TYPE_SAMPLER,
        },
        VK_SHADER_STAGE_COMPUTE_BIT );

    // Uniform assignments
    engine::update_descriptor_set_uniform(
        vulkan, engine, terrain.uniform_desc_set, *info.camera_buffer, 0 );
    engine::update_descriptor_set_uniform(
        vulkan, engine, terrain.uniform_desc_set, *info.debug_buffer, 1 );

    // Material image assignments
    engine::update_descriptor_set_rwimage( vulkan, engine, terrain.texture_desc_set,
        *info.GBuffer_Position, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0 );
    engine::update_descriptor_set_rwimage( vulkan, engine, terrain.texture_desc_set,
        *info.GBuffer_Normal, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1 );
    engine::update_descriptor_set_image(
        vulkan, engine, terrain.texture_desc_set, terrain.test_layer_mask, 2 );
    engine::update_descriptor_set_image(
        vulkan, engine, terrain.texture_desc_set, terrain.grass_albedo_roughness, 3 );
    engine::update_descriptor_set_image(
        vulkan, engine, terrain.texture_desc_set, terrain.grass_normal_ao, 4 );
    engine::update_descriptor_set_rwimage( vulkan, engine, terrain.texture_desc_set,
        *info.color_attachment, VK_IMAGE_LAYOUT_GENERAL, 5 );

    // LUT desc assignments
    engine::update_descriptor_set_image(
        vulkan, engine, terrain.lut_desc_set, info.atmosphere_baker->octahedral_sky, 0 );
    engine::update_descriptor_set_rwimage( vulkan, engine, terrain.lut_desc_set,
        info.atmosphere_baker->octahedral_sky_irradiance, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        1 );

    // Sampler assignments
    engine::update_descriptor_set_sampler(
        vulkan, engine, terrain.sampler_desc_set, vulkan.global_samplers.linear_sampler, 0 );

    engine::Pipeline cs_terrain_lighting_pipeline = engine::create_compute_pipeline( vulkan,
        {
            terrain.uniform_desc_set.layouts[0],
            terrain.texture_desc_set.layouts[0],
            terrain.lut_desc_set.layouts[0],
            terrain.sampler_desc_set.layouts[0],
            terrain.accel_structure_desc_set->layouts[0]
        },
        vk::create::shader_module( vulkan, TERRAIN_SHADER_LIGHTING_MODULE_PATH ),
        "cs_terrain_draw" );

    uint32_t dispatch_x = ( engine.swapchain.extent.width + 7 ) / 8;
    uint32_t dispatch_y = ( engine.swapchain.extent.width + 7 ) / 8;

    glm::ivec3 dispatch_dims = glm::ivec3( dispatch_x, dispatch_y, 1 );

    // Full-screen quad draw.
    engine::ComputeTask cs_terrain_draw_task = {
        cs_terrain_lighting_pipeline,
        {
            &terrain.uniform_desc_set,
            &terrain.texture_desc_set,
            &terrain.lut_desc_set,
            &terrain.sampler_desc_set,
            terrain.accel_structure_desc_set
        },
        dispatch_dims,
    };

    engine::add_cs_task( task_list, cs_terrain_draw_task );
}

}
