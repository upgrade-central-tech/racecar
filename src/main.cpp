#include "context.hpp"
#include "engine/execute.hpp"
#include "engine/pipeline.hpp"
#include "engine/state.hpp"
#include "geometry/triangle.hpp"
#include "scene/scene.hpp"
#include "sdl.hpp"
#include "vk/create.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

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

    {
        std::optional<VkShaderModule> scene_shader_module_opt = vk::create::shader_module(
            ctx.vulkan, "../shaders/world_pos_debug/world_pos_debug.spv" );

        if ( !scene_shader_module_opt ) {
            SDL_Log( "[Engine] Failed to create shader module" );
            return {};
        }

        VkShaderModule& scene_shader_module = scene_shader_module_opt.value();

        std::vector<engine::LayoutResource> scene_layout_resources = {
            { engine.descriptor_system.camera_set_layout, sizeof( vk::mem::CameraBufferData ),
              &engine.descriptor_system.camera_data } };
        std::vector<VkDescriptorSetLayout> scene_layouts = {
            engine.descriptor_system.camera_set_layout };

        std::optional<engine::Pipeline> scene_pipeline_opt = create_gfx_pipeline(
            engine, ctx.vulkan, sceneMesh, scene_layouts, scene_shader_module );

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
                    add_draw_task( task_list, {
                                                  .mesh = sceneMesh,
                                                  .primitive = prim,
                                                  .pipeline = scene_pipeline,
                                                  .shader_module = scene_shader_module,
                                                  .extent = engine.swapchain.extent,
                                                  .clear_screen = true,
                                                  .render_target_is_swapchain = true,
                                              } );
                }
            }
        }
    }

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
