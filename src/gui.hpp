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
        bool ray_traced_shadows = true;
    } debug = {};

    struct DemoData {
        bool rotate_on = false;
        float rotate_speed = 0.005f;
    } demo = {};

    struct AtmosphereData {
        bool animate_zenith = false;
        float radiance_exposure = 5.f;
    } atms = {};
};

Gui initialize( Context& ctx, const engine::State& engine );
void process_event( const SDL_Event* event );
void update( Gui& gui, const camera::OrbitCamera& camera, atmosphere::Atmosphere& atms );
void free();

} // namespace racecar::engine::gui
