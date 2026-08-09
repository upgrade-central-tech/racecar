#include "racecar.hpp"

#define ENABLE_VOLUMETRICS 1
#define ENABLE_TERRAIN 1
#define ENABLE_DEFERRED_AA 1

#include "app_setup.hpp"
#include "atmosphere.hpp"
#include "atmosphere_baker.hpp"
#include "camera_data.hpp"
#include "context.hpp"
#include "deferred.hpp"
#include "demo/car_animation.hpp"
#include "engine/descriptor_set.hpp"
#include "engine/execute.hpp"
#include "engine/pipeline.hpp"
#include "engine/post/anti_aliasing.hpp"
#include "engine/post/ao.hpp"
#include "engine/post/bloom.hpp"
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
#include "passes/debug_texture_pass.hpp"
#include "passes/lighting_pass.hpp"
#include "passes/post_processing.hpp"
#include "passes/reflection_pass.hpp"
#include "passes/scene_pass.hpp"
#include "passes/transparency_pass.hpp"
#include "preset.hpp"
#include "scene/car_rt.hpp"
#include "scene/scene.hpp"
#include "scene/terrain_rt.hpp"
#include "sdl.hpp"
#include "window_events.hpp"

#if ENABLE_TERRAIN
#include "terrain/terrain.hpp"
#endif

#if ENABLE_VOLUMETRICS
#include "volumetrics.hpp"
#endif

#include <chrono>
#include <thread>

namespace racecar {

void run( bool use_fullscreen )
{
    vk::Common& vulkan = vk::Common::GetMut();
    // ================================================================================================================
    // GLOBAL RESOURCE INITIALIZATION
    // ================================================================================================================

    // INITIALIZE VULKAN CONTEXT + ENGINE CONTEXT
    Context ctx = initialize_context( use_fullscreen );
    engine::initialize( ctx );
    engine::State& engine = engine::State::GetMut();
    gui::Gui gui = gui::initialize( ctx );

    // SCENE LOADING/PROCESSING
    scene::Scene scene;
    geometry::scene::Mesh scene_mesh;
    load_scene( &scene, &scene_mesh );

    // GLOBAL UNIFORM BUFFER SETUP
    UniformBuffer<ub_data::Camera> camera_buffer;
    UniformBuffer<ub_data::Debug> debug_buffer;
    engine::DescriptorSet uniform_desc_set;
    load_camera_debug_uniform_buffers( &camera_buffer, &debug_buffer, &uniform_desc_set );

    // GLOBAL TEXTURE SAMPLER SETUP
    VkSampler linear_sampler = VK_NULL_HANDLE;
    VkSampler point_sampler = VK_NULL_HANDLE;
    engine::DescriptorSet sampler_desc_set;
    load_samplers( &linear_sampler, &point_sampler, &sampler_desc_set );

    // LOAD MATERIAL DATA AND BUFFERS
    size_t num_materials = scene.materials.size();
    std::vector<engine::DescriptorSet> material_desc_sets( num_materials );
    std::vector<UniformBuffer<ub_data::Material>> material_uniform_buffers( num_materials );
    load_materials( scene, num_materials, &material_desc_sets, &material_uniform_buffers );

    // LOAD MODEL MATRIX UNIFORM BUFFERS
    size_t num_nodes = scene.nodes.size();
    std::vector<engine::DescriptorSet> model_mat_desc_sets( num_nodes );
    std::vector<UniformBuffer<ub_data::ModelMat>> model_mat_uniform_buffers( num_nodes );
    load_model_mat_uniform_buffers(
        num_nodes,
        &scene,
        &model_mat_desc_sets,
        &model_mat_uniform_buffers
    );

    // LOAD RAYMARCHING TEXTURE SETS
    engine::DescriptorSet raymarch_tex_sets;
    create_raymarch_tex_sets( &raymarch_tex_sets );

    // LOAD MATERIAL LOOKUP TABLES
    engine::DescriptorSet lut_sets;
    vk::mem::AllocatedImage lut_brdf;
    vk::mem::AllocatedImage glint_noise;
    create_lut_sets( &lut_sets, &lut_brdf, &glint_noise );

    // LOAD GLOBAL QUAD MESH
    // TODO: It might be beneficial if meshes like these are global; helps avoid rebuilding them for
    // whatever reason.
    geometry::quad::Mesh quad_mesh = geometry::quad::create();
    geometry::quad::Mesh::instance = &quad_mesh;

    // ================================================================================================================
    // TASK RESOURCE INITIALIZATION
    // ================================================================================================================

    // INITIALIZE GBUFFER
    deferred::GBuffers gbuffers = deferred::initialize_GBuffers();

    // CREATE DEPTH PREPASS PIPELINE
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

    // CREATE MAIN SCENE DRAW PIPELINE
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

    // CREATE SCREEN BUFFERS
    engine::RWImage screen_color;
    engine::RWImage screen_buffer;
    engine::RWImage screen_history;
    create_screen_buffers( &screen_color, &screen_buffer, &screen_history );

    // SETUP ATMOSPHERIC/VOLUMETRIC RESOURCES
    atmosphere::Atmosphere atms = atmosphere::initialize();
    atmosphere::AtmosphereBaker atms_baker = { .atmosphere = &atms };
    volumetric::Volumetric volumetric = volumetric::initialize();
    atmosphere::initialize_atmosphere_baker( atms_baker, volumetric );

    // ================================================================================================================
    // MODEL LOADING
    // ================================================================================================================

    // Load data for each primitive
    std::vector<glm::mat4> transforms;
    std::vector<const scene::Primitive*> prims;
    ub_data::RTTextureUniform rt_texture_uniform = { };
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

    std::vector<const scene::Primitive*> transparent_prims;
    std::vector<const scene::Primitive*> opaque_prims;
    sort_transparent_opaque_prims( prims, scene.materials, &transparent_prims, &opaque_prims );

#if RACECAR_RAY_TRACING
    // BLAS allocation
    ub_data::BLASOffsets blas_offsets;
    alloc_blases( prims, scene_mesh, &blas_offsets );

    // TLAS allocation
    std::vector<vk::rt::Object> objects;
    create_objects( transforms, objects );
    alloc_car_tlas( objects );

    // Create car acceleration structure descriptor set
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

    // Initialize RT texture data uniform buffer
    UniformBuffer<ub_data::RTTextureUniform> rt_texture_uniform_data
        = create_uniform_buffer( rt_texture_uniform, engine.frame_overlap );
    rt_texture_uniform_data.set_data( rt_texture_uniform );
    rt_texture_uniform_data.update_all();

    // Create uniform buffer for BLAS offsets
    UniformBuffer<ub_data::BLASOffsets> offset_data
        = create_uniform_buffer( blas_offsets, static_cast<size_t>( engine.frame_overlap ) );
    offset_data.set_data( blas_offsets );
    offset_data.update_all();

    // Initialize RT vertex data buffer
    vk::mem::AllocatedBuffer padded_vertex_data_buffer
        = create_padded_vertex_data_buffer( scene_mesh );

    UniformBuffer<ub_data::MaterialTable> material_table
        = create_material_table( material_uniform_buffers );

    // Create car descriptor set
    engine::DescriptorSet car_descriptor_set = create_car_desc_set(
        scene_mesh,
        padded_vertex_data_buffer,
        offset_data,
        lut_brdf,
        atms_baker,
        rt_texture_uniform_data,
        material_table
    );

    // Create combined textures descriptor set
    engine::DescriptorSet combined_textures_desc_set
        = create_combined_textures_desc_set( albedo_textures, metallic_roughness_textures );
#endif // RACECAR_RAY_TRACING

    // ================================================================================================================
    // TERRAIN init
    // ================================================================================================================

    geometry::TerrainPrepassInfo prepass_terrain_info = {
        &camera_buffer,
        &debug_buffer,
        &gbuffers,
        &glint_noise,
    };

    geometry::TerrainLightingInfo terrain_lighting_info = {
        &camera_buffer, &debug_buffer, &atms_baker, &gbuffers, &screen_color, &lut_brdf,
    };

    geometry::Terrain test_terrain;
    geometry::initialize_terrain( test_terrain, prepass_terrain_info, terrain_lighting_info );

    // ================================================================================================================
    // Additional Resource Creation
    // ================================================================================================================

#if RACECAR_RAY_TRACING
    TerrainRayTracingInfo terrain_rt;
    init_terrain_ray_tracing_info( &terrain_rt, &test_terrain );

    // Set up reflection data
    engine::RWImage reflection_color;
    engine::RWImage reflection_data;
    engine::Pipeline reflection_pipeline;
    engine::DescriptorSet reflection_buffer_desc_set;
    engine::GfxTask reflection_gfx_task;
    create_reflection_pass_resources(
        ReflectionPassDescSets {
            .uniform_desc_set = uniform_desc_set,
            .sampler_desc_set = sampler_desc_set,
            .gbuffer_desc_set = gbuffers.desc_set,
            .car_tlas_desc_set = car_tlas_desc_set,
            .car_desc_set = car_descriptor_set,
            .combined_textures_desc_set = combined_textures_desc_set,
            .terrain_shading_desc_set = terrain_rt.shading_desc_set,
        },
        &reflection_color,
        &reflection_data,
        &reflection_pipeline,
        &reflection_buffer_desc_set,
        &reflection_gfx_task
    );
#endif // RACECAR_RAY_TRACING

    DebugTexturePass debug_texture_pass;
    std::vector<const engine::RWImage*> debug_textures;
#if RACECAR_RAY_TRACING
    debug_textures.push_back( &reflection_color );
#endif // RACECAR_RAY_TRACING
    initialize_debug_texture_pass( debug_texture_pass, screen_buffer, debug_textures );

    // Set up car lighting pass
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
    };

    // Create car lighting pass pipeline
    engine::Pipeline lighting_pass_pipeline;
    create_lighting_pass_resources( lighting_pass_desc_sets, &lighting_pass_pipeline );

    // Create terrain draw pipeline
    geometry::initialize_terrain_draw_pipeline(
        test_terrain
#if RACECAR_RAY_TRACING
        ,
        car_tlas_desc_set,
        reflection_buffer_desc_set
#endif // RACECAR_RAY_TRACING
    );

    // Create transparency pass
    TransparencyPass transparency_pass;
    create_transparency_pass_resources(
        &transparency_pass,
        scene_mesh,
        transparent_prims,
        &uniform_desc_set,
        &material_desc_sets,
        &model_mat_desc_sets,
        &lut_sets,
        &sampler_desc_set
    );

    // ================================================================================================================
    // PRECOMPUTE
    // ================================================================================================================

    VkFence precompute_fence = engine::create_fence();

    VkCommandBuffer& precompute_cmdbuf = engine.frames[0].start_cmdbuf;
    engine::begin_precompute_commandbuffer( &precompute_cmdbuf, precompute_fence );

#if RACECAR_RAY_TRACING
    build_car_blases( precompute_cmdbuf );
    build_car_tlas( precompute_cmdbuf );
#endif // RACECAR_RAY_TRACING

    geometry::terrain_precompute( test_terrain, precompute_cmdbuf );

    atmosphere::atmosphere_baker_precompute( atms_baker, precompute_cmdbuf );

    engine::submit_precompute_cmdbuf( precompute_fence, precompute_cmdbuf );

    // ================================================================================================================
    // TASK LIST POPULATION
    // ================================================================================================================

    engine::TaskList task_list;

#if RACECAR_RAY_TRACING
    engine::add_gpu_task( task_list, [&]( VkCommandBuffer cmd_buf ) {
        update_car_tlas( cmd_buf, objects, prims, model_mat_uniform_buffers );
    } );

    add_terrain_rt_displace_pass( terrain_rt, task_list );
    add_terrain_rt_build_pass( terrain_rt, task_list );
#endif // RACECAR_RAY_TRACING

    // Once all of the essential buffers are setup (GBuffer + Screen buffers), we run a pipeline
    // barrier to ensure sync.
    engine::PipelineBarrierDescriptor top_pipeline_barriers;
    deferred::create_top_pipeline_barriers( gbuffers, screen_color, &top_pipeline_barriers );
    engine::add_pipeline_barrier( task_list, top_pipeline_barriers );

    // Draw the atmosphere
    atmosphere::draw_atmosphere( task_list, atms, screen_color );

    // Draw the volumetric clouds
    volumetric::draw_volumetric( volumetric, task_list, screen_color );

    // Running the atmosphere baker
    atmosphere::dispatch_atmosphere_baker( task_list, lut_sets, atms_baker );

    // Add draw tasks for each primitive to the Prepass Gfx Task and Depth Gfx Task
    engine::GfxTask prepass_gfx_task = create_prepass_gfx_task( gbuffers );
    add_prim_draw_tasks(
        scene_mesh,
        opaque_prims,
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

    // Add our car prepass into the task list
    engine::add_gfx_task( task_list, prepass_gfx_task );

    // Add draw tasks for terrain to its own terrain_prepass_task (submitted internally), and to
    // depth_prepass_ms
    geometry::draw_terrain_prepass( test_terrain, depth_prepass_ms, task_list );

    // Submit depth prepass (car primitives + terrain)
    engine::add_gfx_task( task_list, depth_prepass_ms.depth_ms_gfx_task );

#if RACECAR_RAY_TRACING
    // Pipeline barrier (gbuffer dependency for reflection compute)
    create_deferred_reflection_pipeline_barrier(
        task_list,
        gbuffers,
        reflection_color,
        reflection_data
    );

    // Add reflection task
    engine::add_gfx_task( task_list, reflection_gfx_task );
#endif // RACECAR_RAY_TRACING

    // Lighting
    create_deferred_lighting_pipeline_barrier(
        task_list,
        gbuffers,
#if RACECAR_RAY_TRACING
        reflection_color,
        reflection_data,
#endif // RACECAR_RAY_TRACING
        screen_color
    );

    // Terrain lighting pass
    geometry::draw_terrain( test_terrain, task_list );

    // Transfer screen_color from the terrain compute pass to the car lighting pass
    create_terrain_car_screen_pipeline_barrier( task_list, screen_color );

    // Car lighting pass, writes to screen_color
    car_lighting_pass( lighting_pass_desc_sets, lighting_pass_pipeline, screen_color, task_list );

    // Transition screen color and screen buffer for post processing
    create_screen_buffer_pipeline_barrier( screen_color, screen_buffer, task_list );

    engine::post::AAPass aa_pass;
    engine::post::AoPass ao_pass;
    engine::post::BloomPass bloom_pass;
    engine::post::TonemappingPass tm_pass;

    // Bloom and AO only on opaque geometry
    pre_transparency_post_passes(
        camera_buffer,
        gbuffers,
        screen_color,
        screen_buffer,
        task_list,
        ao_pass,
        bloom_pass
    );

    // screen_buffer compute write -> colour attachment
    // GBuffer Depth -> write.
    create_transparency_pipeline_barrier( screen_buffer, gbuffers.GBuffer_Depth, task_list );

    execute_transparency_pass(
        &transparency_pass,
        scene_mesh,
        transparent_prims,
        &screen_buffer,
        &gbuffers.GBuffer_Depth,
        camera_buffer,
        model_mat_uniform_buffers,
        task_list
    );

    post_transparency_post_passes(
        camera_buffer,
        gbuffers,
        screen_color,
        screen_buffer,
        screen_history,
        task_list,
        aa_pass,
        tm_pass
    );

    add_debug_texture_pass( debug_texture_pass, task_list );

    // Transition screen buffer to be ready for swapchain blit
    create_screen_buffer_present_pipeline_barrier( screen_buffer, task_list );

    // Blit screen buffer to the swapchain, ready for presentation
    engine::add_blit_task( task_list, { &screen_buffer } );

    // ================================================================================================================
    // MAIN RUNNER LOOP
    // ================================================================================================================

    bool will_quit = false;
    bool stop_drawing = false;
    SDL_Event event = { };
    std::chrono::steady_clock::time_point current_tick;

    while ( !will_quit ) {
        current_tick = std::chrono::steady_clock::now();

        // Handle QUIT, MINIMIZED, RESTORED events
        handle_sdl_window_events(
            ctx,
            gui,
            material_uniform_buffers,
            atms,
            will_quit,
            stop_drawing,
            event
        );

        // Don't draw if we're minimized
        if ( stop_drawing ) {
            std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
            continue;
        }

        engine::begin_frame();

        // Handle preset transitioning
        if ( gui.preset.transition.has_value() ) {
            update_preset_transition( gui, material_uniform_buffers, atms );
        }

        // Camera input, then demo-driven camera motion, then the derived matrices
        camera::process_input( engine.camera );
        apply_demo_camera_motion(
            engine.camera,
            gui,
            scene,
            model_mat_uniform_buffers,
            volumetric
        );

        CameraData camera_data = get_camera_data();

        camera::OrbitCamera& camera = engine.camera;

        // Update camera uniform buffer
        update_camera_uniform_buffer( camera_buffer, camera_data );

        // Update atmosphere uniform buffer
        atmosphere::update_atmosphere_uniform_buffer( gui, atms, camera_data );

        // AO update
        engine::post::update_ao_uniform_buffer( gui, ao_pass );

        // Tonemapping update
        engine::post::update_tonemapping_uniform_buffer( gui, tm_pass );

        // AA update
        engine::post::update_aa_uniform_buffer( aa_pass );

#if ENABLE_VOLUMETRICS
        // Update volumetric camera buffer
        volumetric::update_volumetric_uniform_buffer( atms, volumetric, camera_data );
#endif

        // Update debug uniform buffer
        update_debug_uniform_buffer( gui, atms, debug_buffer );

        // update materials
        update_material_uniform_buffers( gui, material_uniform_buffers, num_materials );

        for ( UniformBuffer<ub_data::ModelMat>& model_mat_buffer : model_mat_uniform_buffers ) {
            model_mat_buffer.update( engine.get_frame_index() );
        }

#if RACECAR_RAY_TRACING
        // Update ray tracing uniform buffers
        update_rt_uniform_buffers(
            offset_data,
            rt_texture_uniform_data,
            material_table,
            material_uniform_buffers
        );
#endif // RACECAR_RAY_TRACING

        // Update terrain
        geometry::update_terrain_uniform_buffer( gui, test_terrain );

        // Scene node transforms, sharing one `discovered` set so a node is only propagated once
        std::vector<bool> discovered = std::vector<bool>( scene.nodes.size(), false );

        update_car_transform( gui, scene, model_mat_uniform_buffers, volumetric, discovered );

        // wheel rotation
        update_wheel_transforms( gui, scene, model_mat_uniform_buffers, discovered );

        // Update bloom settings
        engine::post::update_bloom_uniform_buffer( gui, bloom_pass );

        update_debug_texture_uniform_buffer( debug_texture_pass, gui.debug.texture_exposure );

        gui::update( gui, atms, camera, material_uniform_buffers );

        engine::execute( task_list, gui );
        engine.rendered_frames = engine.rendered_frames + 1;
        engine.frame_number = ( engine.rendered_frames + 1 ) % engine.frame_overlap;

        // Make new screen visible
        SDL_UpdateWindowSurface( ctx.window );

        auto new_tick = std::chrono::steady_clock::now();
        auto duration
            = std::chrono::duration_cast<std::chrono::milliseconds>( new_tick - current_tick );

        // Convert milliseconds to seconds
        engine.delta = static_cast<double>( duration.count() ) * 0.001;
        engine.time += engine.delta;
        current_tick = new_tick;
    }

    // ================================================================================================================
    // RESOURCE CLEANUP
    // ================================================================================================================

    vkDeviceWaitIdle( vulkan.device );
    gui::free();
    engine::free();
    vulkan.destructor_stack.execute_cleanup();
    vk::free();
    sdl::free( ctx.window );
}

}
