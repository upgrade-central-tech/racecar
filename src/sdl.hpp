#pragma once

#include <SDL3/SDL.h>
#include <volk.h>
#include <VkBootstrap.h>

#include <optional>

namespace Racecar::SDL {

struct Context {
  SDL_Window* window = nullptr;
  SDL_Surface* sdl_surface = nullptr;
  SDL_Surface* bryce = nullptr;
};

std::optional<Context> initialize_context(int screen_w, int screen_h, bool fullscreen);

/// Draws the surface onto the window.
void draw(const Context& ctx);

/// Frees everything and shuts down SDL.
void clean_up(Context& ctx);

}  // namespace Racecar::SDL
