#include "sdl.hpp"

namespace Racecar::SDL {

std::optional<Context> initialize(int screen_w, int screen_h, bool fullscreen) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("[SDL] Could not initialize: %s", SDL_GetError());
    return {};
  }

  Context ctx;
  SDL_WindowFlags flags = SDL_WINDOW_VULKAN;

  if (fullscreen) {
    flags |= SDL_WINDOW_FULLSCREEN;
  }

  if (ctx.window = SDL_CreateWindow("RACECAR", screen_w, screen_h, flags); !ctx.window) {
    SDL_Log("[SDL] Could not create window: %s", SDL_GetError());
    return {};
  }

  return ctx;
}

void clean_up(Context& ctx) {
  SDL_DestroyWindow(ctx.window);

  ctx = {.window = nullptr};

  SDL_Quit();
}

}  // namespace Racecar::SDL
