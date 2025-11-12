#pragma once

#include "../context.hpp"
#include "state.hpp"

#include <SDL3/SDL.h>
#include <vulkan/vulkan_core.h>

#include <optional>

namespace racecar::engine::gui {

/// Stores GUI state. Add more fields here for data that you wish to keep track of
/// across frames, and then let ImGui take a reference to it.
struct Gui {
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

    // Later on when Saahil gets multiple buffers to work relatively easily,
    // we can just have a struct called DebugSettings that's used to store all bools here.
    bool enable_albedo_map = true;
    bool enable_normal_map = false;
    bool enable_roughess_metal_map = false;

    bool normals_only = false;
    bool albedo_only = false;
    bool roughness_metal_only = false;

    bool rotate_on = false;
    float rotate_speed = 0.005f;
};

std::optional<Gui> initialize( Context& ctx, const State& engine );
void process_event( const SDL_Event* event );
void update( Gui& gui );
void free();

}  // namespace racecar::engine::gui
