#include "engine.hpp"

#include <SDL3/SDL_vulkan.h>

namespace Racecar {

namespace {

bool initialize_vulkan_instance(Engine& engine) {
  vkb::InstanceBuilder instance_builder;
  instance_builder.set_app_name("RACECAR").set_app_version(1, 0, 0).require_api_version(1, 4);

  vkb::Result<vkb::SystemInfo> system_info_ret = vkb::SystemInfo::get_system_info();

  if (!system_info_ret) {
    SDL_Log("[vkb] Unable to get system info");
    return false;
  }

  vkb::SystemInfo& system_info = system_info_ret.value();

#if DEBUG
  if (system_info.validation_layers_available) {
    instance_builder.enable_validation_layers().use_default_debug_messenger();
  }
#endif

  uint32_t extension_count = 0;
  if (const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
      !extensions) {
    SDL_Log("[SDL] Could not get necessary instance-level extensions: %s", SDL_GetError());
    return false;
  } else {
    SDL_Log("[SDL] Enabling necessary instance extensions:");

    for (uint32_t i = 0; i < extension_count; ++i) {
      const char* extension = extensions[i];
      SDL_Log("- %s", extension);

      if (system_info.is_extension_available(extension)) {
        instance_builder.enable_extension(extension);
      } else {
        SDL_Log("[vkb] Necessary instance extension not available: %s", extension);
        return false;
      }
    }
  }

  vkb::Result<vkb::Instance> instance_ret = instance_builder.build();

  if (!instance_ret) {
    SDL_Log("[vkb] Failed to create Vulkan instance. Error: %s",
            instance_ret.error().message().c_str());
    return false;
  }

  engine.instance = instance_ret.value();
  volkLoadInstance(engine.instance);

  return true;
}

}  // namespace

std::optional<Engine> initialize_engine(const SDL::Context& ctx) {
  Engine engine;

  if (!initialize_vulkan_instance(engine)) {
    SDL_Log("[Engine] Failed to initialize Vulkan instance");
    return {};
  }

  // Note: while we use SDL to create the Vulkan surface, we will use vk-bootstrap to delete it
  if (!SDL_Vulkan_CreateSurface(ctx.window, engine.instance, nullptr, &engine.surface)) {
    SDL_Log("[SDL] Could not create Vulkan surface: %s", SDL_GetError());
    return {};
  }

  return engine;
}

void draw([[maybe_unused]] const SDL::Context& ctx) {}

void clean_up(Engine& engine) {
  // Note: while we use vk-bootstrap to delete the Vulkan surface, we used SDL to create it
  vkb::destroy_surface(engine.instance, engine.surface);
  vkb::destroy_instance(engine.instance);

  engine = {
      .instance = {},
      .surface = nullptr,
  };
}

}  // namespace Racecar
