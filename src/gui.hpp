#pragma once

#include "context.hpp"
#include "engine/state.hpp"

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
        glm::vec4 color = glm::vec4(0.85f, 0.0f, 0.0f, 1.0f);
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
    } debug = {};

    struct DemoData {
        bool rotate_on = false;
        float rotate_speed = 0.005f;
    } demo = {};
};

Gui initialize( Context& ctx, const engine::State& engine );
void process_event( const SDL_Event* event );
void update( Gui& gui );
void free();

} // namespace racecar::engine::gui
