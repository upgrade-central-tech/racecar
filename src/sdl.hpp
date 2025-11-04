#pragma once

#include <SDL3/SDL.h>
#include <volk.h>
#include <VkBootstrap.h>

#include <optional>

namespace Racecar::SDL {

struct Context {
  SDL_Window* window = nullptr;
};

std::optional<Context> initialize(int screen_w, int screen_h, bool fullscreen);

/// Must be called after `Racecar::clean_up` to properly free Vulkan resources.
void clean_up(Context& ctx);

}  // namespace Racecar::SDL
