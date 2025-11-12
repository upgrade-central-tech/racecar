#include "constants.hpp"
#include "context.hpp"
#include "engine/execute.hpp"
#include "engine/gui.hpp"
#include "engine/images.hpp"
#include "engine/pipeline.hpp"
#include "engine/state.hpp"
#include "engine/task_list.hpp"
#include "engine/uniform_buffer.hpp"
#include "geometry/triangle.hpp"
#include "scene/scene.hpp"
#include "sdl.hpp"
#include "vk/create.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <chrono>
#include <cstdlib>
#include <thread>

using namespace racecar;

constexpr bool USE_FULLSCREEN = false;
constexpr const char* GLTF_FILE_PATH = "../assets/sponza/Sponza.gltf";

int main( int, char*[] ) {
    Context ctx;

    if ( std::optional<SDL_Window*> window_opt =
             sdl::initialize( constant::SCREEN_W, constant::SCREEN_H, USE_FULLSCREEN );
         !window_opt ) {
        SDL_Log( "[RACECAR] Could not initialize SDL!" );
        return EXIT_FAILURE;
    } else {
        ctx.window = window_opt.value();
    }

    if ( std::optional<vk::Common> vulkan_opt = vk::initialize( ctx.window ); !vulkan_opt ) {
        SDL_Log( "[RACECAR] Could not initialize Vulkan!" );
        return EXIT_FAILURE;
    } else {
        ctx.vulkan = vulkan_opt.value();
    }

    std::optional<engine::State> engine_opt = engine::initialize( ctx.window, ctx.vulkan );
    if ( !engine_opt ) {
        SDL_Log( "[RACECAR] Failed to initialize engine!" );
        return EXIT_FAILURE;
    }

    engine::State& engine = engine_opt.value();

    std::optional<engine::gui::Gui> gui_opt = engine::gui::initialize( ctx, engine );

    if ( !gui_opt ) {
        SDL_Log( "[RACECAR] Failed to initialize GUI!" );
        return EXIT_FAILURE;
    }

    engine::gui::Gui& gui = gui_opt.value();

    // SCENE LOADING/PROCESSING
    scene::Scene scene;
    geometry::Mesh sceneMesh;
    {
        scene::load_gltf( ctx.vulkan, engine, std::string( GLTF_FILE_PATH ), scene,
                          sceneMesh.vertices, sceneMesh.indices );
        geometry::generate_tangents( sceneMesh );
    }

    std::vector<scene::Texture>& material_textures = scene.textures;
    std::optional<geometry::GPUMeshBuffers> uploaded_mesh_buffer =
        geometry::upload_mesh( ctx.vulkan, engine, sceneMesh.indices, sceneMesh.vertices );
    if ( !uploaded_mesh_buffer ) {
        SDL_Log( "[VMA] Failed to upload mesh" );
    }
    sceneMesh.mesh_buffers = uploaded_mesh_buffer.value();

    // SHADER LOADING/PROCESSING
    std::optional<VkShaderModule> scene_shader_module_opt =
        vk::create::shader_module( ctx.vulkan, "../shaders/pbr/pbr.spv" );
    if ( !scene_shader_module_opt ) {
        SDL_Log( "[Engine] Failed to create shader module" );
        return {};
    }
    VkShaderModule& scene_shader_module = scene_shader_module_opt.value();

    UniformBuffer<uniform_buffer::CameraBufferData> camera_buffer =
        create_uniform_buffer<uniform_buffer::CameraBufferData>(
            ctx.vulkan, engine, {},
            VkShaderStageFlagBits( VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT ),
            int(engine.frame_overlap) );

    // Arbitrary 4 for the max number of images we may need to bind per PBR pass.
    std::vector<vk::mem::AllocatedImage> images =
        std::vector<vk::mem::AllocatedImage>( vk::binding::MAX_IMAGES_BINDED );

    engine::ImagesDescriptor images_descriptor = engine::create_images_descriptor(
        ctx.vulkan, engine, images, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        engine.frame_overlap );

    // Simple set up for linear sampler
    VkSampler nearest_sampler;
    {
        VkSamplerCreateInfo sampler_nearest_create_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_NEAREST,
            .minFilter = VK_FILTER_NEAREST,
        };

        vkCreateSampler( ctx.vulkan.device, &sampler_nearest_create_info, nullptr,
                         &nearest_sampler );
    }

    ctx.vulkan.destructor_stack.push( ctx.vulkan.device, nearest_sampler, vkDestroySampler );

    std::vector<VkSampler> samplers = { nearest_sampler };

    engine::SamplersDescriptor samplers_descriptor = engine::create_samplers_descriptor(
        ctx.vulkan, engine, samplers, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        engine.frame_overlap );

    std::optional<engine::Pipeline> scene_pipeline_opt =
        create_gfx_pipeline( engine, ctx.vulkan, sceneMesh,
                             { camera_buffer.layout( engine.get_frame_index() ),
                               images_descriptor.layout, samplers_descriptor.layout },
                             scene_shader_module );
    if ( !scene_pipeline_opt ) {
        SDL_Log( "[Engine] Failed to create pipeline" );
        return false;
    }

    engine::Pipeline& scene_pipeline = scene_pipeline_opt.value();

    engine::TaskList task_list;

    for ( std::unique_ptr<scene::Node>& node : scene.nodes ) {
        if ( node->mesh.has_value() ) {
            const std::unique_ptr<scene::Mesh>& mesh = node->mesh.value();

            for ( const scene::Primitive& prim : mesh->primitives ) {
                const scene::Material& current_material = scene.materials[size_t(prim.material_id)];
                std::vector<std::optional<scene::Texture>> textures_needed;

                // Lowkey I might refactor this later. Assume the default pipeline is a PBR
                // Albedo Map Pipeline
                if ( current_material.type == scene::MaterialTypes::PBR_ALBEDO_MAP_MAT_TYPE ) {
                    std::optional<int> albedo_index = current_material.base_color_texture_index;

                    std::optional<int> normal_index = current_material.normal_texture_index;

                    std::optional<int> metallic_roughness_index =
                        current_material.metallic_roughness_texture_index;

                    textures_needed.push_back(
                        albedo_index ? std::optional{ ( material_textures[size_t(albedo_index.value())] ) }
                                     : std::nullopt );
                    textures_needed.push_back(
                        normal_index ? std::optional{ ( material_textures[size_t(normal_index.value())] ) }
                                     : std::nullopt );
                    textures_needed.push_back(
                        metallic_roughness_index
                            ? std::optional{ (
                                  material_textures[size_t(metallic_roughness_index.value())] ) }
                            : std::nullopt );

                }  // We would continue this if-else chain for all material pipelines based on
                   // needed textures.

                // TODO: Not yet connected with a layout
                if ( scene.hdri_index.has_value() ) {
                    textures_needed.push_back( scene.textures[scene.hdri_index.value()] );
                } else {
                    textures_needed.push_back( std::nullopt );
                }

                std::vector<vk::mem::AllocatedImage> textures_sent;
                for ( std::optional<scene::Texture>& texture : textures_needed ) {
                    if ( texture && ( textures_sent.size() < images.size() ) ) {
                        textures_sent.push_back( texture->data.value() );
                    }
                }

                engine::DrawResourceDescriptor draw_descriptor =
                    engine::DrawResourceDescriptor::from_mesh( sceneMesh, prim );
                add_draw_task( task_list, {
                                              .draw_resource_descriptor = draw_descriptor,
                                              .images_descriptor = images_descriptor,
                                              .samplers_descriptor = samplers_descriptor,
                                              .uniform_buffers = { &camera_buffer },
                                              .textures = textures_sent,
                                              .pipeline = scene_pipeline,
                                              .extent = engine.swapchain.extent,
                                              .clear_screen = true,
                                              .render_target_is_swapchain = true,
                                          } );
            }
        }
    }

    bool will_quit = false;
    bool stop_drawing = false;
    SDL_Event event = {};

    while ( !will_quit ) {
        while ( SDL_PollEvent( &event ) ) {
            engine::gui::process_event( &event );
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

        {
            // Update the scene block. Hard-coded goodness.
            uniform_buffer::CameraBufferData scene_camera_data = camera_buffer.get_data();
            camera::OrbitCamera& camera = engine.camera;

            glm::mat4 view = camera::calculate_view_matrix( camera );
            glm::mat4 projection = glm::perspective( camera.fov_y, camera.aspect_ratio,
                                                     camera.near_plane, camera.far_plane );
            projection[1][1] *= -1;

            // float angle = static_cast<float>( engine.rendered_frames ) * 0.001f;  // in radians
            glm::mat4 model = glm::mat4( 1.0f );
            // glm::rotate( glm::mat4( 1.0f ), angle, glm::vec3( 0, 1, 0 ) );  // Y-axis rotation

            scene_camera_data.mvp = projection * view * model;
            scene_camera_data.inv_model = glm::inverse( model );
            scene_camera_data.camera_pos = camera::calculate_eye_position( camera );
            scene_camera_data.color = glm::vec3(
                std::sin( static_cast<float>( engine.rendered_frames ) * 0.01f ), 0.0f, 0.0f );
            camera_buffer.set_data( scene_camera_data );
        }

        engine::gui::update( gui );

        if ( engine::execute( engine, ctx, task_list ) ) {
            engine.rendered_frames = engine.rendered_frames + 1;
            engine.frame_number = ( engine.rendered_frames + 1 ) % engine.frame_overlap;
        }

        // Make new screen visible
        SDL_UpdateWindowSurface( ctx.window );
    }

    vkDeviceWaitIdle( ctx.vulkan.device );

    engine::gui::free();
    engine::free( engine, ctx.vulkan );
    vk::free( ctx.vulkan );
    sdl::free( ctx.window );

    return EXIT_SUCCESS;
}
