#pragma once

#include "atmosphere.hpp"
#include "context.hpp"
#include "engine/state.hpp"
#include "gui_material.hpp"
#include "orbit_camera.hpp"
#include "preset.hpp"

#include <SDL3/SDL_events.h>
#include <imgui.h>
#include <vulkan/vulkan_core.h>

#include <optional>
#include <vector>

namespace racecar::gui {

/// Stores GUI state. Add more fields here for data that you wish to keep track of
/// across frames, and then let ImGui take a reference to it.
struct Gui {
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    ImGuiContext* ctx = nullptr;

    bool show_window = true;

    struct DebugData : public Material {
        bool enable_albedo_map = false;
        bool enable_normal_map = false;
        bool enable_roughness_metal_map = false;

        bool normals_only = false;
        bool albedo_only = false;
        bool roughness_metal_only = false;
        bool ray_traced_shadows = true;
        int current_editing_material = 0;
        bool load_material_into_gui = true;
    } debug = {};

    struct DemoData {
        bool enable_translation = false;
        bool enable_camera_lock_on_car = false;
        bool rotate_on = false;
        float rotate_speed = 0.005f;
        float bumpiness = 0.001f;
    } demo = {};

    struct AtmosphereData {
        bool animate_zenith = false;
        float animate_zenith_speed = 1.f;
        float radiance_exposure = 2.5f;
    } atms = {};

    struct AoData {
        float thickness = 1.0f;
        float radius = 0.324f;
        float offset = 0.0f;
        bool enable_debug = false;
        bool enable_ao = true;
    } ao = {};

    struct TerrainData {
        float gt7_local_shadow_strength = 1.0f;
        float wetness = 0.8f; // temporary param
        float snow = 0.3f;
        float scrolling_speed = 0.0f;

        bool enable_gt7_ao = true;
        bool shadowing_only = false;
        bool roughness_only = false;
    } terrain = {};

    struct TonemappingData {
        enum class Mode : int {
            NONE = 0,
            GT7_SDR,
            GT7_HDR,
            REINHARD,
            ACES,
        } mode
            = Mode::GT7_HDR;

        float hdr_target_luminance = 1'000.f;
    } tonemapping = {};

    struct BloomData {
        bool enable = true;
        float threshold = 1.2f;
        float filter_radius = 0.003f;
    } bloom = {};

    struct AAData {
        enum class Mode : int { NONE = 0, TAA } mode = Mode::TAA;
    } aa = {};

    struct PresetData {
        std::vector<Preset> presets = load_presets();
        std::optional<PresetTransition> transition;
    } preset = {};
};

Gui initialize( Context& ctx, const engine::State& engine );
void process_event(
    Gui& gui, const SDL_Event* event, atmosphere::Atmosphere& atms, camera::OrbitCamera& camera );
void update( Gui& gui, atmosphere::Atmosphere& atms, camera::OrbitCamera& camera );
void free();

void use_preset( const Preset& preset, gui::Gui& gui, atmosphere::Atmosphere& atms,
    camera::OrbitCamera& camera );

} // namespace racecar::engine::gui
