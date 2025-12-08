#include "gui.hpp"

#include "log.hpp"

#include <glm/gtc/constants.hpp>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include <array>
#include <optional>
#include <string_view>

namespace racecar::gui {

static constexpr std::array TONEMAPPING_OPTIONS = std::to_array<std::string_view>( {
    "None",
    "GT7 SDR",
    "GT7 HDR",
    "Reinhard",
    "ACES",
} );

static constexpr std::array AA_OPTIONS = std::to_array<std::string_view>( { "None", "TAA" } );

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

void process_event( Gui& gui, const SDL_Event* event, atmosphere::Atmosphere& atms )
{
    // We may want to expand this function later. For now, it serves to remove any ImGui header
    // includes in non-GUI related files.
    ImGui_ImplSDL3_ProcessEvent( event );

    if ( gui.show_window && ImGui::GetIO().WantCaptureMouse ) {
        return;
    }

    switch ( event->type ) {
    case SDL_EVENT_KEY_DOWN: {
        const SDL_KeyboardEvent& key_event = event->key;

        // Control pressed
        if ( key_event.mod & SDL_KMOD_CTRL ) {
            std::optional<size_t> preset_number_opt;

            switch ( key_event.key ) {
            case SDLK_COMMA: {
                // Toggle GUI
                gui.show_window = !gui.show_window;
                break;
            }

                // clang-format off
            case SDLK_1: preset_number_opt = 1; break;
            case SDLK_2: preset_number_opt = 2; break;
            case SDLK_3: preset_number_opt = 3; break;
            case SDLK_4: preset_number_opt = 4; break;
            case SDLK_5: preset_number_opt = 5; break;
            case SDLK_6: preset_number_opt = 6; break;
            case SDLK_7: preset_number_opt = 7; break;
            case SDLK_8: preset_number_opt = 8; break;
            case SDLK_9: preset_number_opt = 9; break;
            case SDLK_0: preset_number_opt = 10; break;
                // clang-format on
            }

            if ( preset_number_opt.has_value() ) {
                // Switch to preset number, if it exists
                if ( size_t preset_number = preset_number_opt.value();
                    preset_number <= gui.preset.presets.size() ) {
                    const Preset& preset = gui.preset.presets[preset_number - 1];
                    use_preset( preset, gui, atms );
                }
            }
        }

        break;
    }
    }
}

void update( Gui& gui, const camera::OrbitCamera& camera, atmosphere::Atmosphere& atms )
{
    if ( !gui.show_window ) {
        return;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if ( ImGui::Begin( "Configuration", &gui.show_window ) ) {
        const ImGuiIO& io = ImGui::GetIO();
        float average_fps = io.Framerate;
        ImGui::Text( "FPS: %.2f (%.1f ms)", average_fps, 1.f / average_fps * 1000.f );

        glm::vec3 cam_pos = camera::calculate_eye_position( camera );
        ImGui::Text( "Camera: [x=%.1f, y=%.1f, z=%1.f]", cam_pos.x, cam_pos.y, cam_pos.z );

        if ( ImGui::BeginTabBar( "Categories" ) ) {
            if ( ImGui::BeginTabItem( "Materials" ) ) {
                gui.debug.load_material_into_gui
                    = ImGui::InputInt( "Editing Material", &gui.debug.current_editing_material );
                ImGui::ColorEdit4( "Base color", &gui.debug.color[0] );
                ImGui::SliderFloat( "Roughness", &gui.debug.roughness, 0, 1.0f );
                ImGui::SliderFloat( "Metallic", &gui.debug.metallic, 0, 1.0f );
                ImGui::SliderFloat(
                    "Clearcoat Roughness", &gui.debug.clearcoat_roughness, 0, 1.0f );
                ImGui::SliderFloat( "Clearcoat Weight", &gui.debug.clearcoat_weight, 0, 1.0f );

                ImGui::SeparatorText( "Glint Params" );
                ImGui::SliderFloat( "Glintiness", &gui.debug.glintiness, 0, 1.0f );
                ImGui::SliderFloat( "Glint log density", &gui.debug.glint_log_density, 0, 26.0f );
                ImGui::SliderFloat( "Glint roughness", &gui.debug.glint_roughness, 0, 1.0f );
                ImGui::SliderFloat( "Glint randomness", &gui.debug.glint_randomness, 0, 10.0f );

                ImGui::EndTabItem();
            }

            if ( ImGui::BeginTabItem( "Presets" ) ) {
                ImGui::SeparatorText( "Loaded presets" );
                for ( const auto& preset : gui.preset.presets ) {
                    if ( ImGui::Button( "Use" ) ) {
                        use_preset( preset, gui, atms );
                    }

                    ImGui::SameLine();
                    ImGui::Text( "%s", preset.name.c_str() );
                }

                ImGui::EndTabItem();
            }

            if ( ImGui::BeginTabItem( "General" ) ) {
                ImGui::SeparatorText( "Debug" );
                ImGui::Checkbox( "Enable albedo map", &gui.debug.enable_albedo_map );
                ImGui::Checkbox( "Enable normal map", &gui.debug.enable_normal_map );
                ImGui::Checkbox(
                    "Enable roughness + metallic map", &gui.debug.enable_roughness_metal_map );

                ImGui::Checkbox( "Turn on albedo only", &gui.debug.albedo_only );
                ImGui::Checkbox( "Turn on normals only", &gui.debug.normals_only );
                ImGui::Checkbox(
                    "Turn on roughness + metallic only", &gui.debug.roughness_metal_only );
                ImGui::Checkbox( "Ray Traced Shadows", &gui.debug.ray_traced_shadows );

                ImGui::SeparatorText( "Demo Settings" );
                ImGui::Checkbox( "Enable car translation", &gui.demo.enable_translation );
                ImGui::Checkbox( "Enable camera lock on car", &gui.demo.enable_camera_lock_on_car );
                ImGui::Checkbox( "Rotate on", &gui.demo.rotate_on );
                ImGui::Text( "Spin speed:" );
                ImGui::SliderFloat( "Min", &gui.demo.rotate_speed, 0, 0.05f );

                ImGui::EndTabItem();
            }

            if ( ImGui::BeginTabItem( "Terrain" ) ) {
                ImGui::SeparatorText( "Debug" );
                ImGui::Checkbox( "Enable GT7 AO/local shadows", &gui.terrain.enable_gt7_ao );
                ImGui::Checkbox( "Shadowing only", &gui.terrain.shadowing_only );
                ImGui::Checkbox( "Roughness only", &gui.terrain.roughness_only );

                ImGui::SeparatorText( "Toggles" );
                ImGui::SliderFloat(
                    "Local shadow strength", &gui.terrain.gt7_local_shadow_strength, 0.0f, 1.0f );
                ImGui::SliderFloat( "Debug wetness", &gui.terrain.wetness, 0.0f, 1.0f );
                ImGui::SliderFloat( "Debug snow", &gui.terrain.snow, 0.0f, 1.0f );
                ImGui::SliderFloat( "Scrolling speed", &gui.terrain.scrolling_speed, 0.0f, 0.1f );
                ImGui::EndTabItem();
            }

            if ( ImGui::BeginTabItem( "Atmosphere" ) ) {
                ImGui::SeparatorText( "Animation" );
                ImGui::Checkbox( "Animate sun movement", &gui.atms.animate_zenith );
                ImGui::SliderFloat( "Animation speed", &gui.atms.animate_zenith_speed, 0.f, 2.f );

                ImGui::SeparatorText( "Settings" );
                ImGui::SliderFloat( "Sun zenith", &atms.sun_zenith,
                    -( glm::half_pi<float>() + 0.1f ), glm::half_pi<float>() + 0.1f );
                ImGui::SliderFloat( "Sun azimuth", &atms.sun_azimuth, 0.f, glm::two_pi<float>() );
                ImGui::SliderFloat( "Radiance exposure", &gui.atms.radiance_exposure, 0.f, 20.f );

                ImGui::EndTabItem();
            }

            if ( ImGui::BeginTabItem( "AO" ) ) {
                ImGui::Checkbox( "Enable AO", &gui.ao.enable_ao );
                ImGui::Checkbox( "Enable Debug", &gui.ao.enable_debug );
                ImGui::SliderFloat( "Thickness", &gui.ao.thickness, 0.0f, 1.0f );
                ImGui::SliderFloat( "Radius", &gui.ao.radius, 0.0f, 1.0f );
                ImGui::SliderFloat( "Offset", &gui.ao.offset, 0.0f, 1.0f );

                ImGui::EndTabItem();
            }

            if ( ImGui::BeginTabItem( "Post" ) ) {
                ImGui::SeparatorText( "Bloom" );
                ImGui::Checkbox( "Enable", &gui.bloom.enable );
                ImGui::SliderFloat( "Threshold", &gui.bloom.threshold, 0.f, 5.f );
                ImGui::SliderFloat( "Filter radius", &gui.bloom.filter_radius, 0.f, 0.025f );

                ImGui::SeparatorText( "Anti-Aliasing" );
                size_t aa_selected_index = static_cast<size_t>( gui.aa.mode );
                std::string_view aa_preview_value = AA_OPTIONS[aa_selected_index];
                if ( ImGui::BeginCombo( "AA Mode", aa_preview_value.data() ) ) {
                    for ( size_t i = 0; i < AA_OPTIONS.size(); ++i ) {
                        bool is_selected = ( aa_selected_index == i );

                        if ( ImGui::Selectable( AA_OPTIONS[i].data(), is_selected ) ) {
                            aa_selected_index = i;
                        }

                        if ( is_selected ) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }

                    gui.aa.mode = static_cast<Gui::AAData::Mode>( aa_selected_index );
                    ImGui::EndCombo();
                }

                ImGui::SeparatorText( "Tonemapping" );
                size_t selected_index = static_cast<size_t>( gui.tonemapping.mode );
                std::string_view preview_value = TONEMAPPING_OPTIONS[selected_index];
                if ( ImGui::BeginCombo( "Tonemapping Mode", preview_value.data() ) ) {
                    for ( size_t i = 0; i < TONEMAPPING_OPTIONS.size(); ++i ) {
                        bool is_selected = ( selected_index == i );

                        if ( ImGui::Selectable( TONEMAPPING_OPTIONS[i].data(), is_selected ) ) {
                            selected_index = i;
                        }

                        if ( is_selected ) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }

                    gui.tonemapping.mode
                        = static_cast<Gui::TonemappingData::Mode>( selected_index );
                    ImGui::EndCombo();
                }

                bool is_not_gt7_hdr = gui.tonemapping.mode != Gui::TonemappingData::Mode::GT7_HDR;
                if ( is_not_gt7_hdr ) {
                    ImGui::BeginDisabled();
                }
                ImGui::SliderFloat(
                    "HDR luminance target", &gui.tonemapping.hdr_target_luminance, 0.f, 10'000.f );
                if ( is_not_gt7_hdr ) {
                    ImGui::EndDisabled();
                }

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

void use_preset( const Preset& preset, gui::Gui& gui, atmosphere::Atmosphere& atms )
{
    log::info( "[preset] Using preset \"{}\"", preset.name );

    atms.sun_zenith = preset.sun_zenith;
    atms.sun_azimuth = preset.sun_azimuth;

    gui.terrain.wetness = preset.wetness;
    gui.terrain.snow = preset.snow;
    gui.terrain.scrolling_speed = preset.scrolling_speed;
    // gui.terrain.bumpiness = preset.bumpiness;

    for ( const auto& material : preset.materials ) {
        gui.debug.current_editing_material = material.slot;

        const gui::Material& data = material.data;
        gui.debug.color = data.color;
        gui.debug.roughness = data.roughness;
        gui.debug.metallic = data.metallic;
        gui.debug.clearcoat_roughness = data.clearcoat_roughness;
        gui.debug.clearcoat_weight = data.clearcoat_weight;
        gui.debug.glintiness = data.glintiness;
        gui.debug.glint_log_density = data.glint_log_density;
        gui.debug.glint_roughness = data.glint_roughness;
        gui.debug.glint_randomness = data.glint_randomness;
    }
}

} // namespace racecar::engine::gui
