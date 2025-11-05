#pragma once

#include "vk/common.hpp"

#include <SDL3/SDL.h>

namespace racecar {

struct Context {
  SDL_Window* window = nullptr;
  vk::Common vulkan;
};

}  // namespace racecar
