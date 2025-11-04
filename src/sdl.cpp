#include "sdl.hpp"

#include <SDL3/SDL_vulkan.h>

namespace Racecar::SDL {

std::optional<Context> initialize(int screen_w, int screen_h, [[maybe_unused]] bool fullscreen) {
  static std::string image_path = "../assets/bryce.bmp";

  if (volkInitialize() != VK_SUCCESS) {
    SDL_Log("[volk] Could not load the Vulkan loader; is it installed on the system?");
    return {};
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("[SDL] Could not initialize: %s", SDL_GetError());
    return {};
  }

  Context ctx;

  if (ctx.window = SDL_CreateWindow("RACECAR", screen_w, screen_h, SDL_WINDOW_VULKAN);
      !ctx.window) {
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
