#pragma once

#include "atmosphere.hpp"
#include "context.hpp"
#include "engine/state.hpp"
#include "orbit_camera.hpp"

#include <SDL3/SDL_events.h>
#include <imgui.h>
#include <vulkan/vulkan_core.h>

namespace racecar::gui {

/// Stores GUI state. Add more fields here for data that you wish to keep track of
/// across frames, and then let ImGui take a reference to it.
struct Gui {
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    ImGuiContext* ctx = nullptr;

    struct DebugData {
        glm::vec4 color = glm::vec4( 0.85f, 0.0f, 0.0f, 1.0f );
        float roughness;
        float metallic;
        float clearcoat_roughness = 0.30f;
        float clearcoat_weight = 1.0f;

        bool enable_albedo_map = false;
        bool enable_normal_map = false;
        bool enable_roughness_metal_map = false;

        bool normals_only = false;
        bool albedo_only = false;
        bool roughness_metal_only = false;
        bool enable_bloom = true;
        bool ray_traced_shadows = true;
        int current_editing_material = 0;
        bool load_material_into_gui = true;
    } debug = {};

    struct DemoData {
        bool rotate_on = false;
        float rotate_speed = 0.005f;
    } demo = {};

    struct AtmosphereData {
        bool animate_zenith = false;
        float radiance_exposure = 2.25f;
    } atms = {};

    struct AoData {
        float thickness = 1.0f;
        float radius = 0.20f;
        float offset = 0.20f;
        bool enable_debug = false;
        bool enable_ao = false;
    } ao = {};

    struct TonemappingData {
        enum class Mode : int {
            NONE,
            SDR,
            HDR,
        } mode
            = Mode::HDR;

        float hdr_target_luminance = 10000.f;
    } tonemapping = {};
};

Gui initialize( Context& ctx, const engine::State& engine );
void process_event( const SDL_Event* event );
void update( Gui& gui, const camera::OrbitCamera& camera, atmosphere::Atmosphere& atms );
void free();

} // namespace racecar::engine::gui
