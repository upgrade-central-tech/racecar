#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <chrono>
#include <cstdlib>
#include <optional>
#include <string>
#include <thread>

constexpr int SCREEN_WIDTH = 1280;
constexpr int SCREEN_HEIGHT = 720;

namespace {

struct Context {
  SDL_Window* window = nullptr;
  SDL_Surface* screen_surface = nullptr;
  SDL_Surface* bryce = nullptr;
};

/// Starts SDL and creates window
std::optional<Context> initialize() {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("Could not init SDL: %s\n", SDL_GetError());
    return {};
  }

  Context ctx;

  if (ctx.window = SDL_CreateWindow("RACECAR", SCREEN_WIDTH, SCREEN_HEIGHT, 0); !ctx.window) {
    SDL_Log("Could not create window: %s\n", SDL_GetError());
    return {};
  }

  ctx.screen_surface = SDL_GetWindowSurface(ctx.window);

  return ctx;
}

bool load_bryce(Context* ctx) {
  static std::string image_path = "../assets/bryce.bmp";

  if (ctx->bryce = SDL_LoadBMP(image_path.c_str()); !ctx->bryce) {
    SDL_Log("Unable to load image \"%s\"! Error: %s\n", image_path.c_str(), SDL_GetError());
    return false;
  }

  return true;
}

void draw(Context* ctx) {
  // Clear screen to white
  SDL_FillSurfaceRect(ctx->screen_surface, nullptr,
                      SDL_MapSurfaceRGB(ctx->screen_surface, 0xff, 0xff, 0xff));

  // Render image to screen
  SDL_BlitSurface(ctx->bryce, nullptr, ctx->screen_surface, nullptr);
}

/// Frees everything and shuts down SDL
void clean_up(Context* ctx) {
  SDL_DestroySurface(ctx->bryce);
  SDL_DestroyWindowSurface(ctx->window);

  *ctx = {
      .window = nullptr,
      .screen_surface = nullptr,
      .bryce = nullptr,
  };

  SDL_Quit();
}

}  // namespace

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  std::optional<Context> ctx_opt = initialize();

  if (!ctx_opt) {
    SDL_Log("Could not initialize!\n");
    return EXIT_FAILURE;
  }

  Context& ctx = ctx_opt.value();

  if (!load_bryce(&ctx)) {
    SDL_Log("Could not load Bryce!\n");
    return EXIT_FAILURE;
  }

  bool will_quit = false;
  bool stop_rendering = false;
  SDL_Event event;

  while (!will_quit) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        will_quit = true;
      }

      if (event.type == SDL_EVENT_WINDOW_MINIMIZED) {
        stop_rendering = true;
      } else if (event.type == SDL_EVENT_WINDOW_RESTORED) {
        stop_rendering = false;
      }
    }

    // Don't draw if we're minimized
    if (stop_rendering) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    draw(&ctx);

    // Make new screen visible
    SDL_UpdateWindowSurface(ctx.window);
  }

  clean_up(&ctx);

  return EXIT_SUCCESS;
}
