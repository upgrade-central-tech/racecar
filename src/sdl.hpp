#pragma once

#include <SDL3/SDL.h>

#include <optional>

namespace Racecar::SDL {

struct Context {
  SDL_Window* window = nullptr;
};

/// Initializes SDL, creates a window, and returns it in a context.
std::optional<Context> initialize(int screen_w, int screen_h, bool fullscreen);

/// Must be called after `Racecar::clean_up` to properly free Vulkan resources.
void clean_up(Context& ctx);

}  // namespace Racecar::SDL
