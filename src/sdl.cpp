#include "sdl.hpp"

namespace racecar::sdl {

std::optional<SDL_Window*> initialize(int screen_w, int screen_h, bool fullscreen) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("[SDL] Could not initialize: %s", SDL_GetError());
    return {};
  }

  SDL_WindowFlags flags = SDL_WINDOW_VULKAN;

  if (fullscreen) {
    flags |= SDL_WINDOW_FULLSCREEN;
  }

  if (SDL_Window* window = SDL_CreateWindow("RACECAR", screen_w, screen_h, flags); !window) {
    SDL_Log("[SDL] Could not create window: %s", SDL_GetError());
    return {};
  } else {
    return window;
  }
}

void free(SDL_Window* window) {
  SDL_DestroyWindow(window);
  SDL_Quit();
}

}  // namespace racecar::sdl
