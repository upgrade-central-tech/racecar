#include "sdl.hpp"

#include <SDL3/SDL_vulkan.h>

#include <string>

namespace Racecar::SDL {

std::optional<Context> initialize_context(int screen_w,
                                          int screen_h,
                                          [[maybe_unused]] bool fullscreen) {
  if (volkInitialize() != VK_SUCCESS) {
    SDL_Log("[volk] Could not init because Vulkan loader isn't installed on the system");
    return {};
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("[SDL] Could not init: %s", SDL_GetError());
    return {};
  }

  Context ctx;

  if (ctx.window = SDL_CreateWindow("RACECAR", screen_w, screen_h, SDL_WINDOW_VULKAN);
      !ctx.window) {
    SDL_Log("[SDL] Could not create window: %s", SDL_GetError());
    return {};
  }

  ctx.sdl_surface = SDL_GetWindowSurface(ctx.window);

  return ctx;
}

bool load_bryce(Context& ctx) {
  static std::string image_path = "../assets/bryce.bmp";

  if (ctx.bryce = SDL_LoadBMP(image_path.c_str()); !ctx.bryce) {
    SDL_Log("[SDL_LoadBMP] Unable to load \"%s\"! Error: %s", image_path.c_str(), SDL_GetError());
    return false;
  }

  return true;
}

void draw(const Context& ctx) {
  // Clear screen to white
  SDL_FillSurfaceRect(ctx.sdl_surface, nullptr,
                      SDL_MapSurfaceRGB(ctx.sdl_surface, 0xff, 0xff, 0xff));

  // Render image to screen
  SDL_BlitSurface(ctx.bryce, nullptr, ctx.sdl_surface, nullptr);
}

void clean_up(Context& ctx) {
  SDL_DestroySurface(ctx.bryce);
  SDL_DestroyWindowSurface(ctx.window);
  SDL_DestroyWindow(ctx.window);

  ctx = {
      .window = nullptr,
      .sdl_surface = nullptr,
      .bryce = nullptr,
  };

  SDL_Quit();
}

}  // namespace Racecar::SDL
