#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <volk.h>
#include <VkBootstrap.h>

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
  vkb::Instance instance;
};

/// Creates Vulkan instance, window, and loads all Vulkan function pointers.
std::optional<Context> initialize() {
  if (volkInitialize() != VK_SUCCESS) {
    SDL_Log("[volk] Could not init because Vulkan loader isn't installed on the system");
    return {};
  }

  vkb::InstanceBuilder instance_builder;
  instance_builder.set_app_name("RACECAR").set_app_version(1, 0, 0).require_api_version(1, 4);

  vkb::Result<vkb::SystemInfo> system_info_ret = vkb::SystemInfo::get_system_info();

  if (!system_info_ret) {
    SDL_Log("[vkb] Unable to get system info");
    return {};
  }

  vkb::SystemInfo& system_info = system_info_ret.value();

#if DEBUG
  if (system_info.validation_layers_available) {
    instance_builder.enable_validation_layers().use_default_debug_messenger();
  }
#endif

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("[SDL] Could not init: %s", SDL_GetError());
    return {};
  }

  Context ctx;

  if (ctx.window = SDL_CreateWindow("RACECAR", SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_VULKAN);
      !ctx.window) {
    SDL_Log("[SDL] Could not create window: %s", SDL_GetError());
    return {};
  }

  uint32_t extension_count = 0;
  if (const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
      !extensions) {
    SDL_Log("[SDL] Could not get necessary instance-level extensions: %s", SDL_GetError());
    return {};
  } else {
    SDL_Log("[SDL] Enabling necessary instance extensions:");

    for (uint32_t i = 0; i < extension_count; ++i) {
      const char* extension = extensions[i];
      SDL_Log("- %s", extension);

      if (system_info.is_extension_available(extension)) {
        instance_builder.enable_extension(extension);
      } else {
        SDL_Log("[vkb] Necessary instance extension not available: %s", extension);
        return {};
      }
    }
  }

  vkb::Result<vkb::Instance> instance_ret = instance_builder.build();

  if (!instance_ret) {
    SDL_Log("[vkb] Failed to create Vulkan instance. Error: %s",
            instance_ret.error().message().c_str());
    return {};
  }

  ctx.instance = instance_ret.value();
  volkLoadInstance(ctx.instance);

  ctx.screen_surface = SDL_GetWindowSurface(ctx.window);

  return ctx;
}

bool load_bryce(Context* ctx) {
  static std::string image_path = "../assets/bryce.bmp";

  if (ctx->bryce = SDL_LoadBMP(image_path.c_str()); !ctx->bryce) {
    SDL_Log("[SDL_LoadBMP] Unable to load \"%s\"! Error: %s", image_path.c_str(), SDL_GetError());
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
  SDL_DestroyWindow(ctx->window);

  vkb::destroy_instance(ctx->instance);

  *ctx = {
      .window = nullptr,
      .screen_surface = nullptr,
      .bryce = nullptr,
      .instance = {},
  };

  SDL_Quit();
}

}  // namespace

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  std::optional<Context> ctx_opt = initialize();

  if (!ctx_opt) {
    SDL_Log("[RACECAR] Could not initialize!");
    return EXIT_FAILURE;
  }

  Context& ctx = ctx_opt.value();

  if (!load_bryce(&ctx)) {
    SDL_Log("[RACECAR] Could not load Bryce!");
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
