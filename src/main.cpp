#include "context.hpp"
#include "engine/execute.hpp"
#include "engine/pipeline.hpp"
#include "engine/state.hpp"
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

constexpr int SCREEN_W = 1280;
constexpr int SCREEN_H = 720;
constexpr bool USE_FULLSCREEN = false;
constexpr const char* GLTF_FILE_PATH = "../assets/suzanne.glb";

int main( int, char*[] ) {
    Context ctx;

    if ( std::optional<SDL_Window*> window_opt =
             sdl::initialize( SCREEN_W, SCREEN_H, USE_FULLSCREEN );
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
    engine::TaskList task_list;

    scene::Scene scene;
    geometry::Mesh sceneMesh;
    scene::load_gltf( std::string( GLTF_FILE_PATH ), scene, sceneMesh.vertices, sceneMesh.indices );

    std::optional<geometry::GPUMeshBuffers> uploaded_mesh_buffer =
        geometry::upload_mesh( ctx.vulkan, engine, sceneMesh.indices, sceneMesh.vertices );
    if ( !uploaded_mesh_buffer ) {
        SDL_Log( "[VMA] Failed to upload mesh" );
    }
    sceneMesh.mesh_buffers = uploaded_mesh_buffer.value();

    // Rendering setup
    // {
    std::optional<VkShaderModule> scene_shader_module_opt =
        vk::create::shader_module( ctx.vulkan, "../shaders/world_pos_debug/world_pos_debug.spv" );
    if ( !scene_shader_module_opt ) {
        SDL_Log( "[Engine] Failed to create shader module" );
        return {};
    }
    VkShaderModule& scene_shader_module = scene_shader_module_opt.value();

    // To use buffers in your shader, you need to add a descriptor_layout.
    // Can be done in the following steps:
    //
    //  (pre-step) Add a struct in uniform_buffers.hpp that will represent your uniform data.
    //  Then make a new member of just that in descriptor_system struct
    //
    //  1. Add a new VkDescriptorSetLayout in descriptors.hpp descriptor_system
    //  2. In the initlaize for descriptor_system, create a new DescriptorBuilder
    //  3. Add necessary bindings to the DescriptorBuilder (this will be your uniform buffers
    //     added to descriptor_system struct)
    //  4. Set your layout = descriptor_layout_builder::build(), and use it as seen below
    //
    // To link a uniform buffer to your shader, do the following:
    //  1. Create a scene_layout_resources vector of type LayoutResource with all layouts
    //  necessary for your pipeline.
    //     Add this to your new task in the struct constructor under .layout_resources
    //  2. I probably did something stupid but make an extra vector of VkDescriptorSetLayout.
    //  This gets passed into
    //     create_gfx_pipeline().
    //
    // If you've done that, then your new uniform buffer should be properly allocated and linked
    // to the draw via vkCmdBindDescriptorSets!
    //
    // This should eventually be replaced with a simpler system
    // like create_uniform
    // create_uniform to actually create a uniform buffer
    // std::vector<engine::LayoutResource> scene_layout_resources = {
    //     { engine.descriptor_system.camera_set_layout,
    //       sizeof( uniform_buffer::CameraBufferData ), &engine.descriptor_system.camera_data } };
    // std::vector<VkDescriptorSetLayout> scene_layouts = {
    //     engine.descriptor_system.camera_set_layout };

    UniformBuffer<uniform_buffer::CameraBufferData> camera_buffer =
        create_uniform_buffer<uniform_buffer::CameraBufferData>(
            ctx.vulkan, engine, {},
            VkShaderStageFlagBits( VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT ),
            engine.frame_overlap );

    std::optional<engine::Pipeline> scene_pipeline_opt =
        create_gfx_pipeline( engine, ctx.vulkan, sceneMesh,
                             { camera_buffer.layout( engine.get_frame_index() ) },
                             scene_shader_module );
    if ( !scene_pipeline_opt ) {
        SDL_Log( "[Engine] Failed to create pipeline" );
        return false;
    }
    engine::Pipeline& scene_pipeline = scene_pipeline_opt.value();

    for ( std::unique_ptr<scene::Node>& node : scene.nodes ) {
        if ( std::holds_alternative<std::unique_ptr<scene::Mesh>>( node->data ) ) {
            std::unique_ptr<scene::Mesh>& mesh =
                std::get<std::unique_ptr<scene::Mesh>>( node->data );
            for ( scene::Primitive& prim : mesh->primitives ) {
                engine::DrawResourceDescriptor desc =
                    engine::DrawResourceDescriptor::from_mesh( sceneMesh, prim );
                add_draw_task( task_list, {
                                              .draw_resource_desc = desc,
                                              .uniform_buffers = { &camera_buffer },
                                              .pipeline = scene_pipeline,
                                              .extent = engine.swapchain.extent,
                                              .clear_screen = true,
                                              .render_target_is_swapchain = true,
                                          } );
            }
        }
    }
    // }

    bool will_quit = false;
    bool stop_drawing = false;
    SDL_Event event;

    while ( !will_quit ) {
        while ( SDL_PollEvent( &event ) ) {
            if ( event.type == SDL_EVENT_QUIT ) {
                will_quit = true;
            }

            if ( event.type == SDL_EVENT_WINDOW_MINIMIZED ) {
                stop_drawing = true;
            } else if ( event.type == SDL_EVENT_WINDOW_RESTORED ) {
                stop_drawing = false;
            }
        }

        // Don't draw if we're minimized
        if ( stop_drawing ) {
            std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
            continue;
        }

        {
            // Update the scene block. Hard-coded goodness.
            uniform_buffer::CameraBufferData& scene_camera_data = camera_buffer.data;

            glm::mat4 view = glm::lookAt( engine.global_camera.eye, engine.global_camera.look_at,
                                          engine.global_camera.up );

            /// TODO: Is there any reason why most of these camera struct values are doubles? Do we
            /// really need that much precision?
            glm::mat4 projection =
                glm::perspective( static_cast<float>( engine.global_camera.fov_y ),
                                  static_cast<float>( engine.global_camera.aspect_ratio ),
                                  static_cast<float>( engine.global_camera.near_plane ),
                                  static_cast<float>( engine.global_camera.far_plane ) );

            projection[1][1] *= -1;

            // glm::mat4 model = glm::mat4( 1.0f );

            float angle = static_cast<float>( engine.rendered_frames ) * 0.001f;  // in radians
            glm::mat4 model =
                glm::rotate( glm::mat4( 1.0f ), angle, glm::vec3( 0, 1, 0 ) );  // Y-axis rotation

            scene_camera_data.mvp = projection * view * model;
            scene_camera_data.inv_model = glm::inverse( model );
            scene_camera_data.color = glm::vec3(
                std::sin( static_cast<uint32_t>( engine.rendered_frames ) * 0.01f ), 0.0f, 0.0f );
        }

        camera_buffer.update( ctx.vulkan, engine.get_frame_index() );
        if ( engine::execute( engine, ctx, task_list ) ) {
            engine.rendered_frames = engine.rendered_frames + 1;
            engine.frame_number = ( engine.rendered_frames + 1 ) % engine.frame_overlap;
        }

        // Make new screen visible
        SDL_UpdateWindowSurface( ctx.window );
    }

    vkDeviceWaitIdle( ctx.vulkan.device );

    engine::free( engine, ctx.vulkan );
    vk::free( ctx.vulkan );
    sdl::free( ctx.window );

    return EXIT_SUCCESS;
}
