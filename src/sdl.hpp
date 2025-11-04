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

std::optional<Context> initialize(int screen_w, int screen_h, bool fullscreen);
void clean_up(Context& ctx);

}  // namespace Racecar::SDL
