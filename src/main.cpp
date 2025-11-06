#include "context.hpp"
#include "sdl.hpp"
#include "engine/state.hpp"
#include "engine/execute.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <chrono>
#include <cstdlib>
#include <thread>

using namespace racecar;

constexpr int SCREEN_W = 1280;
constexpr int SCREEN_H = 720;
constexpr bool USE_FULLSCREEN = false;

int main(int, char*[]) {
	Context ctx;

	if (std::optional<SDL_Window*> window_opt = sdl::initialize(SCREEN_W, SCREEN_H, USE_FULLSCREEN);
		!window_opt) {
		SDL_Log("[RACECAR] Could not initialize SDL!");
		return EXIT_FAILURE;
	} else {
		ctx.window = window_opt.value();
	}

	if (std::optional<vk::Common> vulkan_opt = vk::initialize(ctx.window); !vulkan_opt) {
		SDL_Log("[RACECAR] Could not initialize Vulkan!");
		return EXIT_FAILURE;
	} else {
		ctx.vulkan = vulkan_opt.value();
	}

    std::optional<engine::State> engine_opt = engine::initialize( ctx.window, ctx.vulkan);

    if (!engine_opt) {
        SDL_Log("[RACECAR] Failed to initialize engine!");
        return EXIT_FAILURE;
    }

    engine::State& engine = engine_opt.value();

	bool will_quit = false;
	bool stop_drawing = false;
	SDL_Event event;

	while (!will_quit) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				will_quit = true;
			}

			if (event.type == SDL_EVENT_WINDOW_MINIMIZED) {
				stop_drawing = true;
			} else if (event.type == SDL_EVENT_WINDOW_RESTORED) {
				stop_drawing = false;
			}
		}

		// Don't draw if we're minimized
		if (stop_drawing) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
        }

		if (engine::execute(engine, ctx)) {
			engine.rendered_frames = engine.rendered_frames + 1;
			engine.frame_number = (engine.rendered_frames + 1) % engine.frame_overlap;
		}

		// Make new screen visible
		SDL_UpdateWindowSurface(ctx.window);
	}

	vkDeviceWaitIdle(ctx.vulkan.device);

    engine::free(engine, ctx.vulkan);
	vk::free(ctx.vulkan);
	sdl::free(ctx.window);

	return EXIT_SUCCESS;
}
