#include "terrain.hpp"

#include "../engine/gfx_task.hpp"
#include "../engine/images.hpp"
#include "../engine/task_list.hpp"
#include "../geometry/gpu_mesh_buffers.hpp"
#include "../gui.hpp"
#include "../vk/create.hpp"

const std::filesystem::path TERRAIN_SHADER_PREPASS_MODULE_PATH
    = "../shaders/terrain/terrain_prepass.spv";
const std::filesystem::path TERRAIN_SHADER_LIGHTING_MODULE_PATH
    = "../shaders/terrain/cs_terrain_draw.spv";

// TEST FILE PATHS...
const std::filesystem::path TEST_LAYER_MASK_PATH = "../assets/LUT/test_terrain_map.bmp";
const std::filesystem::path TEST_GRASS_ALBEDO_ROUGHNESS_PATH
    = "../assets/terrain/better_grass/grass_albedo_roughness.png";
const std::filesystem::path TEST_GRASS_NORMAL_AO_PATH
    = "../assets/terrain/better_grass/grass_normal_ao.png";

const std::filesystem::path TEST_ASPHALT_ALBEDO_ROUGHNESS_PATH
    = "../assets/terrain/asphalt_albedo_roughness.png";
const std::filesystem::path TEST_ASPHALT_NORMAL_AO_PATH = "../assets/terrain/asphalt_normal_ao.png";

const std::filesystem::path TERRAIN_NOISE_PAPTH = "../assets/LUT/terrain_noise.jpg";

const float TERRAIN_TILE_WIDTH = 10.0f;
const size_t TERRAIN_NUM_TILES = 50;

namespace racecar::geometry {

void initialize_terrain(
    vk::Common& vulkan,
    engine::State& engine,
    Terrain& terrain,
    const TerrainPrepassInfo& prepass_info,
    const TerrainLightingInfo& lighting_info
)
{
    // Generate enough information for just one planar quad. Expand it later on arbitrarily
    [[maybe_unused]] int32_t size = 1;

    // float scale = 10.0f;
    float offset_y = 0.0f;

    terrain.vertices.clear();
    terrain.indices.clear();

    const float half_width = ( TERRAIN_NUM_TILES * TERRAIN_TILE_WIDTH ) / 2.0f;

    // Generate vertices
    for ( size_t z = 0; z <= TERRAIN_NUM_TILES; ++z ) {
        for ( size_t x = 0; x <= TERRAIN_NUM_TILES; ++x ) {
            float xpos = float( x ) * TERRAIN_TILE_WIDTH - half_width;
            float zpos = float( z ) * TERRAIN_TILE_WIDTH - half_width;
            terrain.vertices.push_back(
                { glm::vec3(
                      xpos,
                      ( x == TERRAIN_NUM_TILES && z == TERRAIN_NUM_TILES ) ? offset_y + 1.0
                                                                           : offset_y,
                      zpos
                  ),
                  glm::vec3( 0, 1, 0 ) }
            );
        }
    }

    // Generate indices
    for ( size_t z = 0; z < TERRAIN_NUM_TILES; ++z ) {
        for ( size_t x = 0; x < TERRAIN_NUM_TILES; ++x ) {
            unsigned int top_left = (unsigned int)( z * ( TERRAIN_NUM_TILES + 1 ) + x );
            unsigned int top_right = (unsigned int)( top_left + 1 );
            unsigned int bottom_left = (unsigned int)( ( z + 1 ) * ( TERRAIN_NUM_TILES + 1 ) + x );
            unsigned int bottom_right = (unsigned int)( bottom_left + 1 );

            terrain.indices.push_back( top_left ); // [0] TL
            terrain.indices.push_back( top_right ); // [1] TR
            terrain.indices.push_back( bottom_left ); // [2] BL
            terrain.indices.push_back( bottom_right ); // [3] BR
        }
    }

    // Create respective mesh buffers using CPU data
    geometry::create_mesh_buffers(
        vulkan,
        terrain.mesh_buffers,
        sizeof( TerrainVertex ) * terrain.vertices.size(),
        sizeof( int32_t ) * terrain.indices.size()
    );

    // Upload to GPU
    geometry::upload_mesh_buffers(
        vulkan,
        engine,
        terrain.mesh_buffers,
        terrain.vertices.data(),
        terrain.indices.data()
    );

    for ( size_t z = 0; z < TERRAIN_NUM_TILES; ++z ) {
        for ( size_t x = 0; x < TERRAIN_NUM_TILES; ++x ) {
            unsigned int top_left = (unsigned int)( z * ( TERRAIN_NUM_TILES + 1 ) + x );
            unsigned int top_right = (unsigned int)( top_left + 1 );
            unsigned int bottom_left = (unsigned int)( ( z + 1 ) * ( TERRAIN_NUM_TILES + 1 ) + x );
            unsigned int bottom_right = (unsigned int)( bottom_left + 1 );

            terrain.tri_indices.push_back( top_left ); // [0] TL
            terrain.tri_indices.push_back( bottom_left ); // [2] BL
            terrain.tri_indices.push_back( top_right ); // [1] TR
            terrain.tri_indices.push_back( bottom_left ); // [2] BL
            terrain.tri_indices.push_back( bottom_right ); // [3] BR
            terrain.tri_indices.push_back( top_right ); // [1] TR
        }
    }

    // Create respective mesh buffers using CPU data
    geometry::create_mesh_buffers(
        vulkan,
        terrain.tri_buffers,
        sizeof( TerrainVertex ) * terrain.vertices.size(),
        sizeof( int32_t ) * terrain.tri_indices.size()
    );

    // Upload to GPU
    geometry::upload_mesh_buffers(
        vulkan,
        engine,
        terrain.tri_buffers,
        terrain.vertices.data(),
        terrain.tri_indices.data()
    );

    // Build descriptors
    terrain.prepass_uniform_desc_set = engine::generate_descriptor_set(
        vulkan,
        engine,
        {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // Camera data
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // Debug data
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // Terrain data
        },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
            | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT
    );

    terrain.terrain_uniform
        = create_uniform_buffer<ub_data::TerrainData>( vulkan, { }, engine.frame_overlap );

    terrain.test_layer_mask = engine::load_image(
        TEST_LAYER_MASK_PATH,
        vulkan,
        engine,
        2,
        VK_FORMAT_R8G8_UNORM,
        false
    );

    terrain.grass_albedo_roughness = engine::load_image(
        TEST_GRASS_ALBEDO_ROUGHNESS_PATH,
        vulkan,
        engine,
        4,
        VK_FORMAT_R8G8B8A8_UNORM,
        true
    );

    terrain.grass_normal_ao = engine::load_image(
        TEST_GRASS_NORMAL_AO_PATH,
        vulkan,
        engine,
        4,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        true
    );

    terrain.asphalt_albedo_roughness = engine::load_image(
        TEST_ASPHALT_ALBEDO_ROUGHNESS_PATH,
        vulkan,
        engine,
        4,
        VK_FORMAT_R8G8B8A8_UNORM,
        true
    );

    terrain.asphalt_normal_ao = engine::load_image(
        TEST_ASPHALT_NORMAL_AO_PATH,
        vulkan,
        engine,
        4,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        true
    );

    terrain.terrain_noise
        = engine::load_image( TERRAIN_NOISE_PAPTH, vulkan, engine, 2, VK_FORMAT_R8G8_UNORM, true );

    vk::rt::alloc_blas(
        vulkan.device,
        vulkan.allocator,
        terrain.blas,
        vulkan.ray_tracing_properties,
        { .vertex_buffer = terrain.tri_buffers.vertex_buffer.handle,
          .index_buffer = terrain.tri_buffers.index_buffer.handle,
          .max_vertex = uint32_t( terrain.vertices.size() ) - 1,
          .index_count = uint32_t( terrain.tri_indices.size() ),
          .vertex_offset = uint32_t( 0 ),
          .index_offset = uint32_t( 0 ),
          .vertex_stride = sizeof( geometry::TerrainVertex ) },
        vulkan.destructor_stack
    );

    vk::rt::alloc_tlas(
        vulkan.device,
        vulkan.allocator,
        terrain.tlas,
        vulkan.ray_tracing_properties,
        { vk::rt::Object { .blas = &terrain.blas, .transform = glm::identity<glm::mat4>() } },
        vulkan.destructor_stack
    );

    terrain.terrain_tlas_desc_set = engine::generate_descriptor_set(
        vulkan,
        engine,
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR },
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
    );

    engine::update_descriptor_set_acceleration_structure(
        vulkan,
        engine,
        terrain.terrain_tlas_desc_set,
        terrain.tlas.handle,
        0
    );

    // ============================================================================================
    // Prepass resources
    // ============================================================================================

    engine::update_descriptor_set_uniform(
        vulkan,
        engine,
        terrain.prepass_uniform_desc_set,
        *prepass_info.camera_buffer,
        0
    );
    engine::update_descriptor_set_uniform(
        vulkan,
        engine,
        terrain.prepass_uniform_desc_set,
        *prepass_info.debug_buffer,
        1
    );
    engine::update_descriptor_set_uniform(
        vulkan,
        engine,
        terrain.prepass_uniform_desc_set,
        terrain.terrain_uniform,
        2
    );

    terrain.terrain_prepass_task = {
        .color_attachments = {
            &prepass_info.gbuffers->GBuffer_Position,
            &prepass_info.gbuffers->GBuffer_Normal,
            &prepass_info.gbuffers->GBuffer_Albedo,
            &prepass_info.gbuffers->GBuffer_Packed_Data,
            &prepass_info.gbuffers->GBuffer_Velocity,
        },
        .depth_image = &prepass_info.gbuffers->GBuffer_Depth,
        .extent = engine.swapchain.extent,
    };

    terrain.prepass_texture_desc_set = engine::generate_descriptor_set(
        vulkan,
        engine,
        {
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // LAYER MASK
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // GRASS ALBEDO + ROUGHNESS
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // GRASS NORMAL + AO
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // ASPHALT ALBEDO + ROUGHNESS
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // ASPHALT NOMRAL + AO
        },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
            | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT
    );

    terrain.prepass_sampler_desc_set = engine::generate_descriptor_set(
        vulkan,
        engine,
        {
            VK_DESCRIPTOR_TYPE_SAMPLER,
        },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
            | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT
    );

    engine::update_descriptor_set_image(
        vulkan,
        engine,
        terrain.prepass_texture_desc_set,
        terrain.test_layer_mask,
        0
    );
    engine::update_descriptor_set_image(
        vulkan,
        engine,
        terrain.prepass_texture_desc_set,
        terrain.grass_albedo_roughness,
        1
    );
    engine::update_descriptor_set_image(
        vulkan,
        engine,
        terrain.prepass_texture_desc_set,
        terrain.grass_normal_ao,
        2
    );
    engine::update_descriptor_set_image(
        vulkan,
        engine,
        terrain.prepass_texture_desc_set,
        terrain.asphalt_albedo_roughness,
        3
    );
    engine::update_descriptor_set_image(
        vulkan,
        engine,
        terrain.prepass_texture_desc_set,
        terrain.asphalt_normal_ao,
        4
    );

    engine::update_descriptor_set_sampler(
        vulkan,
        engine,
        terrain.prepass_sampler_desc_set,
        vulkan.global_samplers.linear_sampler,
        0
    );

    terrain.prepass_lut_desc_set = engine::generate_descriptor_set(
        vulkan,
        engine,
        {
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // Glint noise texture
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // Terrain noise texture
        },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    );

    engine::update_descriptor_set_image(
        vulkan,
        engine,
        terrain.prepass_lut_desc_set,
        *prepass_info.glint_noise,
        0
    );
    engine::update_descriptor_set_image(
        vulkan,
        engine,
        terrain.prepass_lut_desc_set,
        terrain.terrain_noise,
        1
    );

    try {
        terrain.terrain_prepass_pipeline = engine::create_gfx_pipeline(
            engine,
            vulkan,
            engine::get_vertex_input_state_create_info( terrain ),
            {
                terrain.prepass_uniform_desc_set.layouts[0],
                terrain.prepass_texture_desc_set.layouts[0],
                terrain.prepass_sampler_desc_set.layouts[0],
                terrain.prepass_lut_desc_set.layouts[0],
            },
            {
                VK_FORMAT_R16G16B16A16_SFLOAT, // POSITION
                VK_FORMAT_R16G16B16A16_SFLOAT, // NORMAL
                VK_FORMAT_R16G16B16A16_SFLOAT, // ALBEDO
                VK_FORMAT_R16G16B16A16_SFLOAT, // PACKED DATA
                VK_FORMAT_R16G16_SFLOAT, // VELOCITY
            },
            VK_SAMPLE_COUNT_1_BIT,
            false,
            true,
            vk::create::shader_module( vulkan, TERRAIN_SHADER_PREPASS_MODULE_PATH ),
            true
        );
    } catch ( const Exception& ex ) {
        log::error( "Failed to create terrain prepass graphics pipeline: {}", ex.what() );
        throw;
    }

    // ============================================================================================
    // Lighting pass resources
    // ============================================================================================

    terrain.uniform_desc_set = engine::generate_descriptor_set(
        vulkan,
        engine,
        {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // Camera data
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // Debug data
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // Terrain data
        },
        VK_SHADER_STAGE_COMPUTE_BIT
    );

    terrain.texture_desc_set = engine::generate_descriptor_set(
        vulkan,
        engine,
        {
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // GBuffer Position
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // GBuffer Normal + AO
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // GBuffer Albedo + Roughness
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // GBuffer Packed data
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        },
        VK_SHADER_STAGE_COMPUTE_BIT
    );

    terrain.lut_desc_set = engine::generate_descriptor_set(
        vulkan,
        engine,
        {
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // Octahedral sky
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // Octahedral irradiance
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // Octahedral sky mips
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // BRDF_LUT
        },
        VK_SHADER_STAGE_COMPUTE_BIT
    );

    terrain.sampler_desc_set = engine::generate_descriptor_set(
        vulkan,
        engine,
        {
            VK_DESCRIPTOR_TYPE_SAMPLER,
        },
        VK_SHADER_STAGE_COMPUTE_BIT
    );

    // Uniform assignments
    engine::update_descriptor_set_uniform(
        vulkan,
        engine,
        terrain.uniform_desc_set,
        *lighting_info.camera_buffer,
        0
    );
    engine::update_descriptor_set_uniform(
        vulkan,
        engine,
        terrain.uniform_desc_set,
        *lighting_info.debug_buffer,
        1
    );
    engine::update_descriptor_set_uniform(
        vulkan,
        engine,
        terrain.uniform_desc_set,
        terrain.terrain_uniform,
        2
    );

    // Material image assignments
    engine::update_descriptor_set_rwimage(
        vulkan,
        engine,
        terrain.texture_desc_set,
        lighting_info.gbuffers->GBuffer_Position,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        0
    );
    engine::update_descriptor_set_rwimage(
        vulkan,
        engine,
        terrain.texture_desc_set,
        lighting_info.gbuffers->GBuffer_Normal,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        1
    );
    engine::update_descriptor_set_rwimage(
        vulkan,
        engine,
        terrain.texture_desc_set,
        lighting_info.gbuffers->GBuffer_Albedo,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        2
    );
    engine::update_descriptor_set_rwimage(
        vulkan,
        engine,
        terrain.texture_desc_set,
        lighting_info.gbuffers->GBuffer_Packed_Data,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        3
    );
    engine::update_descriptor_set_rwimage(
        vulkan,
        engine,
        terrain.texture_desc_set,
        *lighting_info.color_attachment,
        VK_IMAGE_LAYOUT_GENERAL,
        4
    );

    // LUT desc assignments
    engine::update_descriptor_set_image(
        vulkan,
        engine,
        terrain.lut_desc_set,
        lighting_info.atmosphere_baker->octahedral_sky,
        0
    );
    engine::update_descriptor_set_rwimage(
        vulkan,
        engine,
        terrain.lut_desc_set,
        lighting_info.atmosphere_baker->octahedral_sky_irradiance,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        1
    );
    engine::update_descriptor_set_rwimage(
        vulkan,
        engine,
        terrain.lut_desc_set,
        lighting_info.atmosphere_baker->octahedral_sky_mips,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        2
    );
    engine::update_descriptor_set_image(
        vulkan,
        engine,
        terrain.lut_desc_set,
        *lighting_info.lut_brdf,
        3
    );

    // Sampler assignments
    engine::update_descriptor_set_sampler(
        vulkan,
        engine,
        terrain.sampler_desc_set,
        vulkan.global_samplers.linear_sampler,
        0
    );
}

void initialize_terrain_draw_pipeline(
    Terrain& terrain,
    vk::Common& vulkan,
    engine::DescriptorSet& car_tlas_desc_set,
    engine::DescriptorSet& reflection_texture_desc_set
)
{
    terrain.car_tlas_desc_set = &car_tlas_desc_set;
    terrain.reflection_texture_desc_set = &reflection_texture_desc_set;

    terrain.terrain_lighting_pipeline = engine::create_compute_pipeline(
        vulkan,
        { terrain.uniform_desc_set.layouts[0],
          terrain.texture_desc_set.layouts[0],
          terrain.lut_desc_set.layouts[0],
          terrain.sampler_desc_set.layouts[0],
          terrain.car_tlas_desc_set->layouts[0],
          terrain.reflection_texture_desc_set->layouts[0] },
        vk::create::shader_module( vulkan, TERRAIN_SHADER_LIGHTING_MODULE_PATH ),
        "cs_terrain_draw"
    );
}

void terrain_precompute( Terrain& terrain, VkCommandBuffer precompute_cmdbuf )
{
    vk::rt::build_blas( precompute_cmdbuf, terrain.blas );
    vk::rt::build_tlas( precompute_cmdbuf, terrain.tlas );
}

void draw_terrain_prepass(
    Terrain& terrain,
    [[maybe_unused]] engine::DepthPrepassMS& depth_prepass_ms_task,
    engine::TaskList& task_list
)
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
            &terrain.prepass_texture_desc_set,
            &terrain.prepass_sampler_desc_set,
            &terrain.prepass_lut_desc_set,
        },
        .pipeline = terrain.terrain_prepass_pipeline,
    } );

    engine::add_gfx_task( task_list, terrain.terrain_prepass_task );

    // ATM the terrain does not draw to the depth multisampled prepass
    // PushDepthPrepassMS( depth_prepass_ms_task, draw_descriptor );
}

void draw_terrain( Terrain& terrain, engine::State& engine, engine::TaskList& task_list )
{
    // Can we assume the color_attachment, by this point, is in a write-only state?

    uint32_t dispatch_x = ( engine.swapchain.extent.width + 7 ) / 8;
    uint32_t dispatch_y = ( engine.swapchain.extent.width + 7 ) / 8;

    glm::ivec3 dispatch_dims = glm::ivec3( dispatch_x, dispatch_y, 1 );

    // Full-screen quad draw.
    engine::ComputeTask cs_terrain_draw_task = {
        terrain.terrain_lighting_pipeline,
        { &terrain.uniform_desc_set,
          &terrain.texture_desc_set,
          &terrain.lut_desc_set,
          &terrain.sampler_desc_set,
          terrain.car_tlas_desc_set,
          terrain.reflection_texture_desc_set },
        dispatch_dims,
    };

    engine::add_cs_task( task_list, cs_terrain_draw_task );
}

void update_terrain_uniform_buffer(
    Context& ctx, engine::State& engine, gui::Gui& gui, geometry::Terrain& terrain
)
{
    ub_data::TerrainData terrain_ub = terrain.terrain_uniform.get_data();

    // Final param packs the offset

    terrain_ub.packed_data0 = glm::vec4(
        gui.terrain.enable_gt7_ao ? 1.0f : 0.0f,
        gui.terrain.shadowing_only ? 1.0f : 0.0f,
        gui.terrain.roughness_only ? 1.0f : 0.0f,
        0.0f
    );

    terrain_ub.terrain_data0 = glm::vec4(
        gui.terrain.gt7_local_shadow_strength,
        gui.terrain.wetness,
        gui.terrain.snow,
        0.0f
    );

    // TODO: Add time-delta here to ensure consistent visuals across-frames
    glm::vec2 scroll_direction
        = glm::normalize( glm::vec2( terrain_ub.terrain_data1.z, terrain_ub.terrain_data1.w ) );

    //  for now, temp hard-code the UV scrolling direction
    scroll_direction.x = 0.0f;
    scroll_direction.y = 1.0f;

    glm::vec2 offset_XY = gui.terrain.scrolling_speed * scroll_direction
        + glm::vec2( terrain_ub.terrain_data1.x, terrain_ub.terrain_data1.y );

    terrain_ub.terrain_data1 = glm::vec4( offset_XY, scroll_direction );

    terrain.terrain_uniform.set_data( terrain_ub );
    terrain.terrain_uniform.update( ctx.vulkan, engine.get_frame_index() );
}

}
