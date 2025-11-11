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
};

std::optional<Gui> initialize( Context& ctx, const State& engine );
void process_event( const SDL_Event* event );
void update( Gui& gui );
void free();

}  // namespace racecar::engine::gui
