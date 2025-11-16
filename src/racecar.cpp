#include "racecar.hpp"

#include "constants.hpp"
#include "context.hpp"
#include "engine/descriptor_set.hpp"
#include "engine/execute.hpp"
#include "engine/pipeline.hpp"
#include "engine/state.hpp"
#include "engine/task_list.hpp"
#include "engine/uniform_buffer.hpp"
#include "geometry/procedural.hpp"
#include "geometry/quad.hpp"
#include "gui.hpp"
#include "scene/scene.hpp"
#include "sdl.hpp"
#include "vk/create.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <chrono>
#include <filesystem>
#include <string_view>
#include <thread>

namespace racecar {

namespace {

constexpr std::string_view GLTF_FILE_PATH = "../assets/smoother_suzanne.glb";
constexpr std::string_view SHADER_MODULE_PATH = "../shaders/car_mat/car_mat.spv";
constexpr std::string_view TEST_CUBEMAP_PATH = "../assets/cubemaps/test";
constexpr std::string_view BRDF_LUT_PATH = "../assets/LUT/BRDF.bmp";

}

void run( bool use_fullscreen )
{
    Context ctx;
    ctx.window = sdl::initialize( constant::SCREEN_W, constant::SCREEN_H, use_fullscreen ),
    ctx.vulkan = vk::initialize( ctx.window );

    engine::State engine = engine::initialize( ctx );
    gui::Gui gui = gui::initialize( ctx, engine );

    // SCENE LOADING/PROCESSING
    scene::Scene scene;
    geometry::scene::Mesh scene_mesh;
    scene::load_gltf(
        ctx.vulkan, engine, GLTF_FILE_PATH, scene, scene_mesh.vertices, scene_mesh.indices );
    scene_mesh.mesh_buffers = geometry::scene::upload_mesh(
        ctx.vulkan, engine, scene_mesh.indices, scene_mesh.vertices );

    UniformBuffer camera_buffer = create_uniform_buffer<ub_data::Camera>(
        ctx.vulkan, {}, static_cast<size_t>( engine.frame_overlap ) );
    UniformBuffer debug_buffer = create_uniform_buffer<ub_data::Debug>(
        ctx.vulkan, {}, static_cast<size_t>( engine.frame_overlap ) );
    // UniformBuffer raymarch_buffer = create_uniform_buffer<ub_data::RaymarchBufferData>(
    //     ctx.vulkan, {}, static_cast<size_t>( engine.frame_overlap ) );

    engine::DescriptorSet uniform_desc_set = engine::generate_descriptor_set( ctx.vulkan, engine,
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT );

    engine::update_descriptor_set_uniform( ctx.vulkan, engine, uniform_desc_set, camera_buffer, 0 );
    engine::update_descriptor_set_uniform( ctx.vulkan, engine, uniform_desc_set, debug_buffer, 1 );

    // Simple set up for linear sampler
    VkSampler linear_sampler = VK_NULL_HANDLE;
    {
        VkSamplerCreateInfo sampler_nearest_create_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
        };

        vk::check( vkCreateSampler(
                       ctx.vulkan.device, &sampler_nearest_create_info, nullptr, &linear_sampler ),
            "Failed to create sampler" );
        ctx.vulkan.destructor_stack.push( ctx.vulkan.device, linear_sampler, vkDestroySampler );
    }

    engine::DescriptorSet sampler_desc_set = engine::generate_descriptor_set( ctx.vulkan, engine,
        { VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLER,
            VK_DESCRIPTOR_TYPE_SAMPLER },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT );

    engine::update_descriptor_set_sampler(
        ctx.vulkan, engine, sampler_desc_set, linear_sampler, 0 );

    size_t num_materials = scene.materials.size();
    std::vector<engine::DescriptorSet> material_desc_sets( num_materials );

    for ( size_t i = 0; i < num_materials; i++ ) {
        // Generate a separate texture descriptor set for each of the materials
        material_desc_sets[i] = engine::generate_descriptor_set( ctx.vulkan, engine,
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT );
    }

    engine::DescriptorSet raymarch_tex_sets;
    raymarch_tex_sets
        = engine::generate_descriptor_set( ctx.vulkan, engine, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT );

    vk::mem::AllocatedImage test_data_3D = geometry::generate_test_3D( ctx.vulkan, engine );
    engine::update_descriptor_set_image( ctx.vulkan, engine, raymarch_tex_sets, test_data_3D, 0 );

    engine::DescriptorSet lut_sets;
    lut_sets = engine::generate_descriptor_set( ctx.vulkan, engine,
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT );

    vk::mem::AllocatedImage test_cubemap
        = geometry::create_cubemap( TEST_CUBEMAP_PATH, ctx.vulkan, engine );
    vk::mem::AllocatedImage lut_brdf
        = geometry::load_image( BRDF_LUT_PATH, ctx.vulkan, engine, 2, VK_FORMAT_R16G16_SFLOAT );
    engine::update_descriptor_set_image( ctx.vulkan, engine, lut_sets, test_cubemap, 0 );
    engine::update_descriptor_set_image( ctx.vulkan, engine, lut_sets, lut_brdf, 1 );

    engine::Pipeline scene_pipeline;

    try {
        size_t frame_index = engine.get_frame_index();
        scene_pipeline = create_gfx_pipeline( engine, ctx.vulkan,
            engine::get_vertex_input_state_create_info( scene_mesh ),
            {
                uniform_desc_set.layouts[frame_index],
                material_desc_sets[0].layouts[frame_index],
                lut_sets.layouts[frame_index],
                sampler_desc_set.layouts[frame_index],
            },
            vk::create::shader_module( ctx.vulkan, SHADER_MODULE_PATH ) );
    } catch ( const Exception& ex ) {
        log::error( "Failed to create graphics pipeline: {}", ex.what() );
        throw;
    }

    geometry::quad::Mesh quad_mesh = geometry::quad::create( ctx.vulkan, engine );

    [[maybe_unused]] VkPipelineVertexInputStateCreateInfo test
        = engine::get_vertex_input_state_create_info( quad_mesh );

    engine::TaskList task_list;

    {
        engine::GfxTask sponza_gfx_task = {
            .clear_color = { { { 0.f, 0.f, 1.f, 1.f } } },
            .clear_depth = 1.f,
            .render_target_is_swapchain = true,
            .extent = engine.swapchain.extent,
        };

        for ( const std::unique_ptr<scene::Node>& node : scene.nodes ) {
            if ( node->mesh.has_value() ) {
                const std::unique_ptr<scene::Mesh>& mesh = node->mesh.value();

                for ( const scene::Primitive& prim : mesh->primitives ) {
                    const scene::Material& current_material
                        = scene.materials[static_cast<size_t>( prim.material_id )];
                    std::vector<std::optional<scene::Texture>> textures_needed;

                    switch ( current_material.type ) {
                    case scene::MaterialType::PBR_ALBEDO_MAP: {
                        std::optional<int> albedo_index = current_material.base_color_texture_index;
                        std::optional<int> normal_index = current_material.normal_texture_index;
                        std::optional<int> metallic_roughness_index
                            = current_material.metallic_roughness_texture_index;

                        textures_needed.push_back( albedo_index
                                ? std::optional { ( scene
                                          .textures[static_cast<size_t>( albedo_index.value() )] ) }
                                : std::nullopt );
                        textures_needed.push_back( normal_index
                                ? std::optional { ( scene
                                          .textures[static_cast<size_t>( normal_index.value() )] ) }
                                : std::nullopt );
                        textures_needed.push_back( metallic_roughness_index
                                ? std::optional { ( scene.textures[static_cast<size_t>(
                                      metallic_roughness_index.value() )] ) }
                                : std::nullopt );

                        break;
                    }

                    default:
                        throw Exception( "[main_gfx_task] Unhandled material type" );
                    }

                    // TODO: Not yet connected with a layout
                    if ( scene.hdri_index.has_value() ) {
                        textures_needed.push_back( scene.textures[scene.hdri_index.value()] );
                    } else {
                        textures_needed.push_back( std::nullopt );
                    }

                    std::vector<vk::mem::AllocatedImage> textures_sent;

                    for ( std::optional<scene::Texture>& texture : textures_needed ) {
                        if ( texture
                            && ( textures_sent.size() < vk::binding::MAX_IMAGES_BINDED ) ) {
                            textures_sent.push_back( texture->data.value() );
                        }
                    }

                    // Actually bind the texture handle to the descriptorset
                    for ( size_t i = 0; i < textures_sent.size(); i++ ) {
                        engine::update_descriptor_set_image( ctx.vulkan, engine,
                            material_desc_sets[static_cast<size_t>( prim.material_id )],
                            textures_sent[i], static_cast<int>( i ) );
                    }

                    engine::DrawResourceDescriptor draw_descriptor
                        = engine::DrawResourceDescriptor::from_mesh(
                            scene_mesh.mesh_buffers.vertex_buffer.handle,
                            scene_mesh.mesh_buffers.index_buffer.handle,
                            static_cast<uint32_t>( scene_mesh.indices.size() ), prim );

                    // give the material descriptor set to the draw task
                    sponza_gfx_task.draw_tasks.push_back( {
                        .draw_resource_descriptor = draw_descriptor,
                        .descriptor_sets = {
                            &uniform_desc_set,
                            &material_desc_sets[static_cast<size_t>( prim.material_id )],
                            &lut_sets,
                            &sampler_desc_set,
                        },
                        .pipeline = scene_pipeline,
                    } );
                }
            }
        }

        engine::add_gfx_task( task_list, sponza_gfx_task );
    }

    bool will_quit = false;
    bool stop_drawing = false;
    SDL_Event event = {};

    while ( !will_quit ) {
        while ( SDL_PollEvent( &event ) ) {
            gui::process_event( &event );
            camera::process_event( ctx, &event, engine.camera );

            if ( event.type == SDL_EVENT_QUIT ) {
                will_quit = true;
            }

            if ( event.type == SDL_EVENT_WINDOW_MINIMIZED ) {
                stop_drawing = true;
            } else if ( event.type == SDL_EVENT_WINDOW_RESTORED ) {
                stop_drawing = false;
            }
        }

        camera::process_input( engine.camera );

        // Don't draw if we're minimized
        if ( stop_drawing ) {
            std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
            continue;
        }

        // Update camera uniform buffer
        {
            ub_data::Camera camera_ub = camera_buffer.get_data();
            camera::OrbitCamera& camera = engine.camera;

            glm::mat4 view = camera::calculate_view_matrix( camera );
            glm::mat4 projection = glm::perspective(
                camera.fov_y, camera.aspect_ratio, camera.near_plane, camera.far_plane );
            projection[1][1] *= -1;

            glm::mat4 model = !gui.demo.rotate_on
                ? camera_ub.model
                : glm::rotate( camera_ub.model, gui.demo.rotate_speed, glm::vec3( 0, 1, 0 ) );

            camera_ub.mvp = projection * view * model;
            camera_ub.model = model;
            camera_ub.inv_model = glm::inverse( model );
            camera_ub.camera_pos = camera::calculate_eye_position( camera );
            camera_ub.color = glm::vec3( 0.85f, 0.0f, 0.0f );

            camera_buffer.set_data( camera_ub );
            camera_buffer.update( ctx.vulkan, engine.get_frame_index() );
        }

        // Update debug uniform buffer
        {
            ub_data::Debug debug_ub = {
                .color = gui.debug.color,
                .packed_data0 = glm::vec4( gui.debug.roughness, gui.debug.metallic,
                    gui.debug.clearcoat_roughness, gui.debug.clearcoat_weight ),
                .enable_albedo_map = gui.debug.enable_albedo_map,
                .enable_normal_map = gui.debug.enable_normal_map,
                .enable_roughness_metal_map = gui.debug.enable_roughness_metal_map,
                .normals_only = gui.debug.normals_only,
                .albedo_only = gui.debug.albedo_only,
                .roughness_metal_only = gui.debug.roughness_metal_only,
            };

            debug_buffer.set_data( debug_ub );
            debug_buffer.update( ctx.vulkan, engine.get_frame_index() );
        }

        // {
        //     ub_data::RaymarchBufferData raymarch_ub = {
        //         .step_size = 1,
        //     };

        //     raymarch_buffer.set_data( raymarch_ub );
        //     raymarch_buffer.update( ctx.vulkan, engine.get_frame_index() );
        // }

        gui::update( gui );

        engine::execute( engine, ctx, task_list );
        engine.rendered_frames = engine.rendered_frames + 1;
        engine.frame_number = ( engine.rendered_frames + 1 ) % engine.frame_overlap;

        // Make new screen visible
        SDL_UpdateWindowSurface( ctx.window );
    }

    vkDeviceWaitIdle( ctx.vulkan.device );
    gui::free();
    engine::free( engine );
    ctx.vulkan.destructor_stack.execute_cleanup();
    vk::free( ctx.vulkan );
    sdl::free( ctx.window );
}

}
