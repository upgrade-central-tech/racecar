#pragma once

#include "sdl.hpp"

#include <volk.h>
#include <VkBootstrap.h>

#include <optional>

namespace Racecar {

struct Engine {
  vkb::Instance instance;
  VkSurfaceKHR surface = nullptr;
};

std::optional<Engine> initialize_engine(const SDL::Context& ctx);
void draw(const SDL::Context& ctx);
void clean_up(Engine& engine);

}  // namespace Racecar
