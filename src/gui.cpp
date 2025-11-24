#include "gui.hpp"

#include "log.hpp"

#include <glm/gtc/constants.hpp>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

namespace racecar::gui {

Gui initialize( Context& ctx, const engine::State& engine )
{
    Gui gui;

    {
        VkDescriptorPoolSize pool_size = {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE,
        };

        VkDescriptorPoolCreateInfo descriptor_pool_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets = pool_size.descriptorCount,
            .poolSizeCount = 1,
            .pPoolSizes = &pool_size,
        };

        vk::check( vkCreateDescriptorPool(
                       ctx.vulkan.device, &descriptor_pool_info, nullptr, &gui.descriptor_pool ),
            "[gui] Failed to create descriptor pool" );
        ctx.vulkan.destructor_stack.push(
            ctx.vulkan.device, gui.descriptor_pool, vkDestroyDescriptorPool );
    }

    IMGUI_CHECKVERSION();
    gui.ctx = ImGui::CreateContext();

    if ( !gui.ctx ) {
        throw Exception( "[ImGui] Failed to create context" );
    }

    // Enable keyboard controls
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    if ( !ImGui_ImplSDL3_InitForVulkan( ctx.window ) ) {
        throw Exception( "[ImGui] Failed to init SDL3 impl for Vulkan" );
    }

    {
        ImGui_ImplVulkan_InitInfo init_info = {
            .ApiVersion = VK_API_VERSION_1_4,
            .Instance = ctx.vulkan.instance,
            .PhysicalDevice = ctx.vulkan.device.physical_device,
            .Device = ctx.vulkan.device,
            .QueueFamily = ctx.vulkan.graphics_queue_family,
            .Queue = ctx.vulkan.graphics_queue,
            .DescriptorPool = gui.descriptor_pool,
            .MinImageCount = engine.swapchain.requested_min_image_count,
            .ImageCount = engine.swapchain.image_count,

            .PipelineInfoMain =
                {
                    .PipelineRenderingCreateInfo =
                        {
                            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                            .colorAttachmentCount = 1,
                            .pColorAttachmentFormats = &engine.swapchain.image_format,
                        },
                },
            .UseDynamicRendering = true,

        };

        if ( !ImGui_ImplVulkan_Init( &init_info ) ) {
            throw Exception( "[ImGui] Failed to init Vulkan impl" );
        }
    }

    log::info( "[gui] Initialized!" );

    return gui;
}

void process_event( const SDL_Event* event )
{
    // We may want to expand this function later. For now, it serves to remove any ImGui header
    // includes in non-GUI related files.
    ImGui_ImplSDL3_ProcessEvent( event );
}

void update( Gui& gui, const camera::OrbitCamera& camera, atmosphere::Atmosphere& atms )
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if ( ImGui::Begin( "Configuration" ) ) {
        const ImGuiIO& io = ImGui::GetIO();
        float average_fps = io.Framerate;
        ImGui::Text( "FPS: %.2f (%.1f ms)", average_fps, 1.f / average_fps * 1000.f );

        glm::vec3 cam_pos = camera::calculate_eye_position( camera );
        ImGui::Text( "Camera: [x=%.1f, y=%.1f, z=%1.f]", cam_pos.x, cam_pos.y, cam_pos.z );

        if ( ImGui::BeginTabBar( "Categories" ) ) {
            if ( ImGui::BeginTabItem( "General" ) ) {
                ImGui::SeparatorText( "Material Settings" );
                ImGui::ColorEdit4( "Base color", &gui.debug.color[0] );
                ImGui::SliderFloat( "Roughness", &gui.debug.roughness, 0, 1.0f );
                ImGui::SliderFloat( "Metallic", &gui.debug.metallic, 0, 1.0f );
                ImGui::SliderFloat(
                    "Clearcoat Roughness", &gui.debug.clearcoat_roughness, 0, 1.0f );
                ImGui::SliderFloat( "Clearcoat Weight", &gui.debug.clearcoat_weight, 0, 1.0f );

                ImGui::SeparatorText( "Debug" );
                ImGui::Checkbox( "Enable albedo map", &gui.debug.enable_albedo_map );
                ImGui::Checkbox( "Enable normal map", &gui.debug.enable_normal_map );
                ImGui::Checkbox(
                    "Enable roughness + metallic map", &gui.debug.enable_roughness_metal_map );

                ImGui::Checkbox( "Turn on albedo only", &gui.debug.albedo_only );
                ImGui::Checkbox( "Turn on normals only", &gui.debug.normals_only );
                ImGui::Checkbox(
                    "Turn on roughness + metallic only", &gui.debug.roughness_metal_only );

                ImGui::SeparatorText( "Demo Settings" );
                ImGui::Checkbox( "Rotate on", &gui.demo.rotate_on );
                ImGui::Text( "Spin speed:" );
                ImGui::SliderFloat( "Min", &gui.demo.rotate_speed, 0, 0.05f );

                ImGui::EndTabItem();
            }

            if ( ImGui::BeginTabItem( "Atmosphere" ) ) {
                ImGui::Checkbox( "Ping-pong zenith", &gui.atms.animate_zenith );
                ImGui::SliderFloat( "Sun azimuth", &atms.sun_azimuth, 0.f, glm::two_pi<float>() );
                ImGui::SliderFloat( "Sun zenith", &atms.sun_zenith, 0.f, glm::pi<float>() );
                ImGui::SliderFloat( "Radiance exposure", &gui.atms.radiance_exposure, 0.f, 20.f );

                ImGui::EndTabItem();
            }

            if ( ImGui::BeginTabItem( "Post" ) ) {
                ImGui::Checkbox( "Enable bloom", &gui.debug.enable_bloom );
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

    ImGui::End();
    ImGui::Render();
}

void free()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

} // namespace racecar::engine::gui
