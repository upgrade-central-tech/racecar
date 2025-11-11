#pragma once

#include "../context.hpp"
#include "state.hpp"

#include <SDL3/SDL.h>
#include <vulkan/vulkan_core.h>

#include <optional>

namespace racecar::engine::gui {

struct Gui {
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
};

std::optional<Gui> initialize( Context& ctx, const State& engine );
void process_events( const SDL_Event* event );
void update( Gui& gui );
void free();

}  // namespace racecar::engine::gui
