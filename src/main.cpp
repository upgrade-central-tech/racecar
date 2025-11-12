#include "constants.hpp"
#include "context.hpp"
#include "engine/descriptor_set.hpp"
#include "engine/execute.hpp"
#include "engine/gui.hpp"
#include "engine/images.hpp"
#include "engine/pipeline.hpp"
#include "engine/state.hpp"
#include "engine/task_list.hpp"
#include "engine/uniform_buffer.hpp"
#include "scene/scene.hpp"
#include "sdl.hpp"
#include "vk/create.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <chrono>
#include <cstdlib>
#include <string_view>
#include <thread>

using namespace racecar;

constexpr bool USE_FULLSCREEN = false;
constexpr std::string_view GLTF_FILE_PATH = "../assets/sponza/Sponza.gltf";

int main( int, char*[] )
{
    Context ctx;

    if ( std::optional<SDL_Window*> window_opt
        = sdl::initialize( constant::SCREEN_W, constant::SCREEN_H, USE_FULLSCREEN );
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
    geometry::Mesh scene_mesh;

    {
        if ( !scene::load_gltf( ctx.vulkan, engine, std::string( GLTF_FILE_PATH ), scene,
                 scene_mesh.vertices, scene_mesh.indices ) ) {
            SDL_Log( "[RACECAR] Failed to load glTF from file path \"%s\"", GLTF_FILE_PATH.data() );
            return EXIT_FAILURE;
        }

        geometry::generate_tangents( scene_mesh );
    }

    std::optional<geometry::GPUMeshBuffers> uploaded_scene_mesh_buffer_opt
        = geometry::upload_mesh( ctx.vulkan, engine, scene_mesh.indices, scene_mesh.vertices );

    if ( !uploaded_scene_mesh_buffer_opt ) {
        SDL_Log( "[RACECAR] Failed to upload scene mesh buffers" );
    }

    scene_mesh.mesh_buffers = uploaded_scene_mesh_buffer_opt.value();

    // SHADER LOADING/PROCESSING
    std::optional<VkShaderModule> scene_shader_module_opt
        = vk::create::shader_module( ctx.vulkan, "../shaders/pbr/pbr.spv" );

    if ( !scene_shader_module_opt ) {
        SDL_Log( "[RACECAR] Failed to create shader module" );
        return EXIT_FAILURE;
    }

    VkShaderModule& scene_shader_module = scene_shader_module_opt.value();

    auto camera_buffer_opt = create_uniform_buffer<uniform_buffer::CameraBufferData>(
        ctx.vulkan, {}, static_cast<size_t>( engine.frame_overlap ) );

    if ( !camera_buffer_opt ) {
        SDL_Log( "[RACECAR] Failed to create camera uniform buffer!" );
        return EXIT_FAILURE;
    }

    UniformBuffer<uniform_buffer::CameraBufferData>& camera_buffer = camera_buffer_opt.value();

    auto camera_descriptor_set_opt = engine::generate_descriptor_set( ctx.vulkan, engine,
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT );

    if ( !camera_descriptor_set_opt ) {
        SDL_Log( "[RACECAR] Failed to generate camera descriptor set" );
        return EXIT_FAILURE;
    }

    engine::DescriptorSet& camera_descriptor_set = camera_descriptor_set_opt.value();

    engine::update_descriptor_set_uniform( ctx.vulkan, engine, camera_descriptor_set,
        camera_buffer.buffer( engine.get_frame_index() ).handle, 0,
        sizeof( uniform_buffer::CameraBufferData ) );

    // Simple set up for linear sampler
    VkSampler nearest_sampler = VK_NULL_HANDLE;
    {
        VkSamplerCreateInfo sampler_nearest_create_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_NEAREST,
            .minFilter = VK_FILTER_NEAREST,
        };

        if ( vkCreateSampler(
                 ctx.vulkan.device, &sampler_nearest_create_info, nullptr, &nearest_sampler ) ) {
            SDL_Log( "[RACECAR] Failed to create sampler" );
            return EXIT_FAILURE;
        }
    }

    ctx.vulkan.destructor_stack.push( ctx.vulkan.device, nearest_sampler, vkDestroySampler );

    std::vector<VkSampler> samplers = { nearest_sampler };

    auto sampler_descriptor_set_opt = engine::generate_descriptor_set( ctx.vulkan, engine,
        { VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLER,
            VK_DESCRIPTOR_TYPE_SAMPLER },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT );

    if ( !sampler_descriptor_set_opt ) {
        SDL_Log( "[RACECAR] Failed to generate texture descriptor set" );
        return EXIT_FAILURE;
    }

    engine::DescriptorSet& sampler_descriptor_opt = sampler_descriptor_set_opt.value();
    

    // get number of materials
    size_t num_materials = scene.materials.size();
    SDL_Log("NUm materials: %d", int(num_materials));

    std::vector<engine::DescriptorSet> material_descriptor_sets(num_materials);

    for (size_t i = 0; i < num_materials; i++) {
        // Albedo, normal, metallic/roughness, test_texture3
        // generate a separate texture descriptor set for each of the materials
        auto textures_descriptor_set_opt = engine::generate_descriptor_set( ctx.vulkan, engine,
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT );

        if ( !textures_descriptor_set_opt ) {
            SDL_Log( "[RACECAR] Failed to generate texture descriptor set" );
            return EXIT_FAILURE;
        }

        SDL_Log("GOOD");

        material_descriptor_sets[i] = textures_descriptor_set_opt.value();
    }

    std::optional<engine::Pipeline> scene_pipeline_opt
        = create_gfx_pipeline( engine, ctx.vulkan, scene_mesh,
            {
                camera_descriptor_set.layouts[engine.get_frame_index()],

                // this is fine: currently, each of our texture sets share the same layout,
                // so we can just pass in one layout into the pipeline and then bind a
                // different descriptorset for each draw_task
                material_descriptor_sets[0].layouts[engine.get_frame_index()],

                sampler_descriptor_opt.layouts[engine.get_frame_index()],
            },
            scene_shader_module );

    if ( !scene_pipeline_opt ) {
        SDL_Log( "[Engine] Failed to create pipeline" );
        return false;
    }

    engine::Pipeline& scene_pipeline = scene_pipeline_opt.value();

    std::vector<scene::Texture>& material_textures = scene.textures;
    engine::TaskList task_list;

    engine::GfxTask main_draw = {
        .clear_screen = true,
        .render_target_is_swapchain = true,
        .extent = engine.swapchain.extent,
    };

    for ( std::unique_ptr<scene::Node>& node : scene.nodes ) {
        if ( node->mesh.has_value() ) {
            const std::unique_ptr<scene::Mesh>& mesh = node->mesh.value();

            for ( const scene::Primitive& prim : mesh->primitives ) {
                const scene::Material& current_material
                    = scene.materials[static_cast<size_t>( prim.material_id )];

                std::vector<std::optional<scene::Texture>> textures_needed;

                if ( current_material.type == scene::MaterialType::PBR_ALBEDO_MAP ) {
                    std::optional<int> albedo_index = current_material.base_color_texture_index;
                    std::optional<int> normal_index = current_material.normal_texture_index;
                    std::optional<int> metallic_roughness_index
                        = current_material.metallic_roughness_texture_index;

                    textures_needed.push_back( albedo_index
                            ? std::optional { (
                                  material_textures[static_cast<size_t>( albedo_index.value() )] ) }
                            : std::nullopt );
                    textures_needed.push_back( normal_index
                            ? std::optional { (
                                  material_textures[static_cast<size_t>( normal_index.value() )] ) }
                            : std::nullopt );
                    textures_needed.push_back( metallic_roughness_index
                            ? std::optional { ( material_textures[static_cast<size_t>(
                                  metallic_roughness_index.value() )] ) }
                            : std::nullopt );

                } // We would continue this if-else chain for all material pipelines based on
                  // needed textures.

                // TODO: Not yet connected with a layout
                if ( scene.hdri_index.has_value() ) {
                    textures_needed.push_back( scene.textures[scene.hdri_index.value()] );
                } else {
                    textures_needed.push_back( std::nullopt );
                }

                std::vector<vk::mem::AllocatedImage> textures_sent;
                for ( std::optional<scene::Texture>& texture : textures_needed ) {
                    if ( texture && ( textures_sent.size() < vk::binding::MAX_IMAGES_BINDED ) ) {
                        textures_sent.push_back( texture->data.value() );
                    }
                }

                // actually bind the texture handle to the descriptorset
                engine::update_descriptor_set_image(ctx.vulkan, engine, material_descriptor_sets[size_t(prim.material_id)], textures_sent[0], 0);
                engine::update_descriptor_set_image(ctx.vulkan, engine, material_descriptor_sets[size_t(prim.material_id)], textures_sent[1], 1);
                engine::update_descriptor_set_image(ctx.vulkan, engine, material_descriptor_sets[size_t(prim.material_id)], textures_sent[2], 2);

                engine::DrawResourceDescriptor draw_descriptor
                    = engine::DrawResourceDescriptor::from_mesh( scene_mesh, prim );

                // give the material descriptor set to the draw task
                main_draw.draw_tasks.push_back( {
                    .draw_resource_descriptor = draw_descriptor,
                    .descriptor_sets = { &camera_descriptor_set, &material_descriptor_sets[size_t(prim.material_id)], &sampler_descriptor_opt },
                    .pipeline = scene_pipeline,
                } );
            }
        }
    }

    engine::add_gfx_task( task_list, main_draw );

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
            glm::mat4 projection = glm::perspective(
                camera.fov_y, camera.aspect_ratio, camera.near_plane, camera.far_plane );
            projection[1][1] *= -1;

            // float angle = static_cast<float>( engine.rendered_frames ) * 0.001f;  // in radians
            glm::mat4 model = ( !gui.rotate_on )
                ? scene_camera_data.model
                : glm::rotate( scene_camera_data.model, gui.rotate_speed,
                      glm::vec3( 0, 1, 0 ) ); // Y-axis rotation

            scene_camera_data.mvp = projection * view * model;
            scene_camera_data.model = model;
            scene_camera_data.inv_model = glm::inverse( model );
            scene_camera_data.camera_pos = camera::calculate_eye_position( camera );
            scene_camera_data.color = glm::vec3(
                std::sin( static_cast<float>( engine.rendered_frames ) * 0.01f ), 0.0f, 0.0f );

            // Debug params
            scene_camera_data.flags0 = glm::ivec4( gui.enable_albedo_map, gui.enable_normal_map,
                gui.enable_roughess_metal_map, gui.normals_only );
            scene_camera_data.flags1
                = glm::ivec4( gui.roughness_metal_only, gui.albedo_only, 0, 0 );

            camera_buffer.set_data( scene_camera_data );
        }

        engine::gui::update( gui );

        camera_buffer.update( ctx.vulkan, engine.get_frame_index() );
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
