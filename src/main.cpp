#include "engine.hpp"
#include "sdl.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <chrono>
#include <cstdlib>
#include <thread>

using namespace Racecar;

constexpr int SCREEN_W = 1280;
constexpr int SCREEN_H = 720;
constexpr bool USE_FULLSCREEN = false;

int main(int, char*[]) {
  std::optional<SDL::Context> ctx_opt = SDL::initialize(SCREEN_W, SCREEN_H, USE_FULLSCREEN);

  if (!ctx_opt) {
    SDL_Log("[RACECAR] Could not initialize!");
    return EXIT_FAILURE;
  }

  SDL::Context& ctx = ctx_opt.value();

  std::optional<Racecar::Engine> engine_opt = Racecar::initialize_engine(ctx);

  if (!engine_opt) {
    SDL_Log("[RACECAR] Could not initialize engine!");
    return EXIT_FAILURE;
  }

  Racecar::Engine& engine = engine_opt.value();

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

    Racecar::draw(ctx);

    // Make new screen visible
    SDL_UpdateWindowSurface(ctx.window);
  }

  Racecar::clean_up(engine);
  SDL::clean_up(ctx);

  return EXIT_SUCCESS;
}
