#pragma once

#include "sdl.hpp"

#include <VkBootstrap.h>

#include <optional>

namespace Racecar {

struct Engine {
  vkb::Instance instance;
  VkSurfaceKHR surface = nullptr;
};

std::optional<Engine> initialize_engine(const SDL::Context& ctx);

bool initialize_vulkan_instance(Engine& engine);
void initialize_swapchain(Engine& engine);

void clean_up(Engine& engine);

}  // namespace Racecar
