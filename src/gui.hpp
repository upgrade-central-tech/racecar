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

        float glintiness = 1.0f;
        float glint_log_density = 22.965f;
        float glint_roughness = 0.51f;
        float glint_randomness = 2.0f;

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
        float radiance_exposure = 7.00f;
    } atms = {};

    struct AoData {
        float thickness = 1.0f;
        float radius = 0.062f;
        float offset = 0.0f;
        bool enable_debug = false;
        bool enable_ao = false;
    } ao = {};

    struct TerrainData {
        bool enable_gt7_ao = true;
        bool shadowing_only;
        bool roughness_only;

        float gt7_local_shadow_strength = 1.0f;
        float wetness = 0.8f; // temporary param
        float snow = 0.3f;
    } terrain = {};

    struct TonemappingData {
        enum class Mode : int {
            NONE = 0,
            GT7_SDR,
            GT7_HDR,
            REINHARD,
            ACES,
        } mode
            = Mode::NONE;

        float hdr_target_luminance = 1'000.f;
    } tonemapping = {};

    struct AAData {
        enum class Mode : int { NONE = 0, TAA } mode = Mode::TAA;
    } aa = {};
};

Gui initialize( Context& ctx, const engine::State& engine );
void process_event( const SDL_Event* event );
void update( Gui& gui, const camera::OrbitCamera& camera, atmosphere::Atmosphere& atms );
void free();

} // namespace racecar::engine::gui
