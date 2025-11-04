#pragma once

#include "vk.hpp"

#include <SDL3/SDL.h>

namespace Racecar {

struct Context {
  SDL_Window* window = nullptr;
  vk::Common vulkan;
};

}  // namespace Racecar
