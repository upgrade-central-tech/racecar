#include "furnace.hpp"

#include "app_setup.hpp"
#include "atmosphere.hpp"
#include "camera_data.hpp"
#include "context.hpp"
#include "deferred.hpp"
#include "engine/descriptor_set.hpp"
#include "engine/execute.hpp"
#include "engine/images.hpp"
#include "engine/pipeline.hpp"
#include "engine/post/ao.hpp"
#include "engine/post/bloom.hpp"
#include "engine/post/temporal_anti_aliasing.hpp"
#include "engine/post/tonemapping.hpp"
#include "engine/precompute.hpp"
#include "engine/prepass.hpp"
#include "engine/state.hpp"
#include "engine/task_list.hpp"
#include "engine/uniform_buffer.hpp"
#include "geometry/quad.hpp"
#include "gui.hpp"
#include "gui_uniforms.hpp"
#include "lut_sets.hpp"
#include "orbit_camera.hpp"
#include "passes/lighting_pass.hpp"
#include "passes/post_processing.hpp"
#include "passes/scene_pass.hpp"
#include "passes/transparency_pass.hpp"
#include "scene/car_rt.hpp"
#include "scene/scene.hpp"
#include "sdl.hpp"
#include "settings.h"
#include "window_events.hpp"

#include <chrono>
#include <cstring>
#include <string>
#include <thread>

namespace racecar {

namespace {

const std::string FURNACE_SPHERE_FILE_PATH = "../assets/furnace_sphere.glb";

// PAPER_WHITE (2.5) displays as 1.0 with tonemapping off. Half of it leaves headroom to see
// energy gain as well as loss.
constexpr float FURNACE_RADIANCE = 1.25f;

}

void furnace( bool use_fullscreen )
{
    vk::Common& vulkan = vk::Common::GetMut();

    // GLOBAL RESOURCE INITIALIZATION

    Context ctx = initialize_context( use_fullscreen );
    engine::initialize( ctx );
    engine::State& engine = engine::State::GetMut();
    gui::Gui gui = gui::initialize( ctx );

    // Scene loading
    scene::Scene scene = {};
    geometry::scene::Mesh scene_mesh;
    scene::load_gltf( FURNACE_SPHERE_FILE_PATH, scene, scene_mesh.vertices, scene_mesh.indices );
    geometry::scene::generate_tangents( scene_mesh );
    scene_mesh.mesh_buffers
        = geometry::scene::upload_mesh( scene_mesh.indices, scene_mesh.vertices );

    // Setup camera and debug buffers
    UniformBuffer<ub_data::Camera> camera_buffer;
    UniformBuffer<ub_data::Debug> debug_buffer;
    engine::DescriptorSet uniform_desc_set;
    load_camera_debug_uniform_buffers( &camera_buffer, &debug_buffer, &uniform_desc_set );

    VkSampler linear_sampler = VK_NULL_HANDLE;
    VkSampler point_sampler = VK_NULL_HANDLE;
    engine::DescriptorSet sampler_desc_set;
    load_samplers( &linear_sampler, &point_sampler, &sampler_desc_set );

    // Load material data, forced to a white untextured albedo
    size_t num_materials = scene.materials.size();
    std::vector<engine::DescriptorSet> material_desc_sets( num_materials );
    std::vector<UniformBuffer<ub_data::Material>> material_uniform_buffers( num_materials );
    load_materials( scene, num_materials, &material_desc_sets, &material_uniform_buffers );

    for ( UniformBuffer<ub_data::Material>& material_buffer : material_uniform_buffers ) {
        ub_data::Material material = material_buffer.get_data();
        material.base_color = glm::vec4( 1.f );
        material.has_base_color_texture = 0;
        material_buffer.set_data( material );
        material_buffer.update_all();
    }

    size_t num_nodes = scene.nodes.size();
    std::vector<engine::DescriptorSet> model_mat_desc_sets( num_nodes );
    std::vector<UniformBuffer<ub_data::ModelMat>> model_mat_uniform_buffers( num_nodes );
    load_model_mat_uniform_buffers(
        num_nodes,
        &scene,
        &model_mat_desc_sets,
        &model_mat_uniform_buffers
    );

    // Load material lookup tables
    engine::DescriptorSet lut_sets;
    vk::mem::AllocatedImage lut_brdf;
    vk::mem::AllocatedImage glint_noise;
    create_lut_sets( &lut_sets, &lut_brdf, &glint_noise );

    // Uniform white sky in place of the baked atmosphere
    glm::vec4 sky_texel( FURNACE_RADIANCE, FURNACE_RADIANCE, FURNACE_RADIANCE, 1.f );
    vk::mem::AllocatedImage white_sky = engine::create_image(
        &sky_texel,
        VkExtent3D( 1, 1, 1 ),
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_TYPE_2D,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        false
    );
    engine::update_descriptor_set_image( lut_sets, white_sky, LUT_INDEX::OCTAHEDRAL_SKY );
    engine::update_descriptor_set_image( lut_sets, white_sky, LUT_INDEX::OCTAHEDRAL_MIPS );

    // Sun fully occluded, so only the sky lights the sphere
    vk::mem::AllocatedBuffer sun_visibility_buffer = vk::mem::create_buffer(
        sizeof( float ),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );
    float sun_visibility = 0.f;
    std::memcpy( sun_visibility_buffer.info.pMappedData, &sun_visibility, sizeof( float ) );
    vmaFlushAllocation( vulkan.allocator, sun_visibility_buffer.allocation, 0, sizeof( float ) );

    engine::DescriptorSet sun_visibility_desc_set = engine::generate_descriptor_set(
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
    );
    engine::update_descriptor_set_const_storage_buffer(
        sun_visibility_desc_set,
        sun_visibility_buffer,
        0
    );

    geometry::quad::Mesh quad_mesh = geometry::quad::create();
    geometry::quad::Mesh::instance = &quad_mesh;

    // TASK RESOURCE INITIALIZATION

    deferred::GBuffers gbuffers = deferred::initialize_GBuffers();

    engine::DescriptorSet depth_uniform_desc_set;
    engine::Pipeline depth_ms_pipeline;
    engine::DepthPrepassMS depth_prepass_ms;
    engine::create_depth_ms_prepass(
        &depth_uniform_desc_set,
        &depth_ms_pipeline,
        camera_buffer,
        scene_mesh,
        &gbuffers,
        &depth_prepass_ms
    );

    engine::Pipeline scene_pipeline;
    create_scene_gfx_pipeline(
        &scene_pipeline,
        scene_mesh,
        &uniform_desc_set,
        &material_desc_sets[0],
        &model_mat_desc_sets[0],
        &lut_sets,
        &sampler_desc_set
    );

    engine::RWImage screen_color;
    engine::RWImage screen_buffer;
    engine::RWImage screen_history;
    create_screen_buffers( &screen_color, &screen_buffer, &screen_history );

    // Only used to satisfy the GUI and to derive a valid sun direction
    atmosphere::Atmosphere atms = atmosphere::initialize();

    // MODEL LOADING

    std::vector<glm::mat4> transforms;
    std::vector<const scene::Primitive*> prims;
    ub_data::RTTextureUniform rt_texture_uniform = {};
    std::vector<vk::mem::AllocatedImage> albedo_textures;
    std::vector<vk::mem::AllocatedImage> metallic_roughness_textures;
    load_model_primitive_material_data(
        scene,
        model_mat_uniform_buffers,
        transforms,
        prims,
        rt_texture_uniform,
        albedo_textures,
        metallic_roughness_textures,
        material_desc_sets
    );

#if RACECAR_RAY_TRACING
    ub_data::BLASOffsets blas_offsets;
    alloc_blases( prims, scene_mesh, &blas_offsets );

    std::vector<vk::rt::Object> objects;
    create_objects( transforms, objects );
    alloc_car_tlas( objects );

    engine::DescriptorSet car_tlas_desc_set = create_accel_structure_desc_set();
    std::vector<VkAccelerationStructureKHR> car_tlas_handles;
    for ( const vk::rt::AccelerationStructure& tlas : engine.tlas ) {
        car_tlas_handles.push_back( tlas.handle );
    }
    engine::update_descriptor_set_acceleration_structure_per_frame(
        car_tlas_desc_set,
        car_tlas_handles,
        0
    );

    // No reflection pass: empty reflections fall back to the sky
    glm::vec4 empty_texel( 0.f );
    vk::mem::AllocatedImage empty_reflection = engine::create_image(
        &empty_texel,
        VkExtent3D( 1, 1, 1 ),
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_TYPE_2D,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        false
    );
    engine::DescriptorSet reflection_buffer_desc_set = engine::generate_descriptor_set(
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
    );
    engine::update_descriptor_set_image( reflection_buffer_desc_set, empty_reflection, 0 );
    engine::update_descriptor_set_image( reflection_buffer_desc_set, empty_reflection, 1 );
#endif // RACECAR_RAY_TRACING

    LightingPassDescSets lighting_pass_desc_sets = {
        .uniform_desc_set = uniform_desc_set,
        .material_desc_sets = material_desc_sets,
        .lut_sets = lut_sets,
        .sampler_desc_set = sampler_desc_set,
        .gbuffer_desc_set = gbuffers.desc_set,
#if RACECAR_RAY_TRACING
        .car_tlas_desc_set = car_tlas_desc_set,
        .reflection_buffer_desc_set = reflection_buffer_desc_set,
#endif // RACECAR_RAY_TRACING
        .sun_visibility_desc_set = sun_visibility_desc_set,
    };

    engine::Pipeline lighting_pass_pipeline;
    create_lighting_pass_resources( lighting_pass_desc_sets, &lighting_pass_pipeline );

    // PRECOMPUTE

#if RACECAR_RAY_TRACING
    VkFence precompute_fence = engine::create_fence();
    VkCommandBuffer& precompute_cmdbuf = engine.frames[0].start_cmdbuf;
    engine::begin_precompute_commandbuffer( &precompute_cmdbuf, precompute_fence );
    build_car_blases( precompute_cmdbuf );
    build_car_tlas( precompute_cmdbuf );
    engine::submit_precompute_cmdbuf( precompute_fence, precompute_cmdbuf );
#endif // RACECAR_RAY_TRACING

    // TASK LIST POPULATION

    engine::TaskList task_list;

    engine::PipelineBarrierDescriptor top_pipeline_barriers;
    deferred::create_top_pipeline_barriers( gbuffers, screen_color, &top_pipeline_barriers );
    engine::add_pipeline_barrier( task_list, top_pipeline_barriers );

    // Background: the same radiance the sky texture holds
    engine::GfxTask background_gfx_task = {
        .clear_color = VkClearColorValue {
            { FURNACE_RADIANCE, FURNACE_RADIANCE, FURNACE_RADIANCE, 1.f } },
        .color_attachments = { &screen_color },
        .extent = engine.swapchain.extent,
    };
    engine::add_gfx_task( task_list, background_gfx_task, "Furnace Background" );

    engine::GfxTask prepass_gfx_task = create_prepass_gfx_task( gbuffers );
    add_prim_draw_tasks(
        scene_mesh,
        prims,
        ScenePassTarget {
            .pipeline = scene_pipeline,
            .gfx_task = prepass_gfx_task,
            .uniform_desc_set = uniform_desc_set,
            .sampler_desc_set = sampler_desc_set,
            .lut_sets = lut_sets,
            .material_desc_sets = material_desc_sets,
            .model_mat_desc_sets = model_mat_desc_sets,
        },
        DepthPassTarget {
            .pipeline = depth_ms_pipeline,
            .gfx_task = depth_prepass_ms.depth_ms_gfx_task,
            .uniform_desc_set = depth_uniform_desc_set,
        }
    );
    engine::add_gfx_task( task_list, prepass_gfx_task, "Prepass" );
    engine::add_gfx_task( task_list, depth_prepass_ms.depth_ms_gfx_task, "Depth Prepass MS" );

    // Round-trips screen_color through the layout the terrain pass would have left it in
    create_deferred_lighting_pipeline_barrier( task_list, gbuffers, screen_color );
    create_terrain_car_screen_pipeline_barrier( task_list, screen_color );

    car_lighting_pass( lighting_pass_desc_sets, lighting_pass_pipeline, screen_color, task_list );

    create_screen_buffer_pipeline_barrier( screen_color, screen_buffer, task_list );

    engine::post::TAAPass taa_pass;
    engine::post::AoPass ao_pass;
    engine::post::BloomPass bloom_pass;
    engine::post::TonemappingPass tm_pass;

    pre_transparency_post_passes(
        camera_buffer,
        gbuffers,
        screen_color,
        screen_buffer,
        task_list,
        ao_pass,
        bloom_pass
    );

    create_transparency_pipeline_barrier( screen_buffer, gbuffers.GBuffer_Depth, task_list );

    post_transparency_post_passes(
        camera_buffer,
        gbuffers,
        screen_color,
        screen_buffer,
        screen_history,
        task_list,
        taa_pass,
        tm_pass
    );

    create_screen_buffer_present_pipeline_barrier( screen_buffer, task_list );
    engine::add_blit_task( task_list, { &screen_buffer }, "Blit To Swapchain" );

    // Keep the furnace linear: no AO darkening, no bloom, no tone curve
    gui.ao.enable_ao = false;
    RuntimeSettings::SetEnabled( RacecarSettings::ENABLE_BLOOM, false );
    RuntimeSettings::Set<RacecarSettings::TONEMAPPING_MODE>( TonemappingMode::NONE );

    // MAIN RUNNER LOOP

    bool will_quit = false;
    bool stop_drawing = false;
    SDL_Event event = {};
    std::chrono::steady_clock::time_point current_tick;

    while ( !will_quit ) {
        current_tick = std::chrono::steady_clock::now();

        handle_sdl_window_events(
            ctx,
            gui,
            material_uniform_buffers,
            atms,
            will_quit,
            stop_drawing,
            event
        );

        if ( stop_drawing ) {
            std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
            continue;
        }

        engine::begin_frame();

        camera::process_input( engine.camera );
        CameraData camera_data = get_camera_data();
        update_camera_uniform_buffer( camera_buffer, camera_data );

        atmosphere::update_atmosphere_uniform_buffer( gui, atms, camera_data );
        engine::post::update_ao_uniform_buffer( gui, ao_pass );
        engine::post::update_tonemapping_uniform_buffer( gui, tm_pass );
        engine::post::update_aa_uniform_buffer( taa_pass );
        engine::post::update_bloom_uniform_buffer( gui, bloom_pass );

        update_debug_uniform_buffer( gui, atms, debug_buffer );
        update_material_uniform_buffers( gui, material_uniform_buffers, num_materials );

        scene::update_model_mat_buffers( scene, model_mat_uniform_buffers );

        gui::update( gui, atms, engine.camera, material_uniform_buffers, task_list );

        engine::execute( task_list, gui );
        engine.rendered_frames = engine.rendered_frames + 1;
        engine.frame_number = ( engine.rendered_frames + 1 ) % engine.frame_overlap;

        SDL_UpdateWindowSurface( ctx.window );

        auto new_tick = std::chrono::steady_clock::now();
        auto duration
            = std::chrono::duration_cast<std::chrono::milliseconds>( new_tick - current_tick );
        engine.delta = static_cast<double>( duration.count() ) * 0.001;
        engine.time += engine.delta;
    }

    // RESOURCE CLEANUP

    vkDeviceWaitIdle( vulkan.device );
    gui::free();
    engine::free();
    vulkan.destructor_stack.execute_cleanup();
    vk::free();
    sdl::free( ctx.window );
}

}
