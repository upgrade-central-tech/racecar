#pragma once

#include "sdl.hpp"

#include <volk.h>
#include <VkBootstrap.h>

#include <optional>
#include <vector>

namespace Racecar {

struct Engine {
  vkb::Instance instance;
  vkb::Device device;
  VkSurfaceKHR surface = nullptr;

  vkb::Swapchain swapchain;
  std::vector<VkImage> swapchain_images;
  std::vector<VkImageView> swapchain_image_views;
};

std::optional<Engine> initialize_engine(const SDL::Context& ctx);
void draw(const SDL::Context& ctx);
void clean_up(Engine& engine);

}  // namespace Racecar
