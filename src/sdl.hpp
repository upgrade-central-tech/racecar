#pragma once

#include <SDL3/SDL.h>

#include <optional>

namespace racecar::sdl {

/// Initializes SDL and creates a window.
std::optional<SDL_Window*> initialize(int screen_w, int screen_h, bool fullscreen);

/// Must be called after `Racecar::clean_up` to properly free Vulkan resources.
void free(SDL_Window* window);

}  // namespace racecar::sdl
