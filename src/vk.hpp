#pragma once

#include <SDL3/SDL.h>
#include <volk.h>
#include <VkBootstrap.h>

#include <optional>

namespace Racecar::vk {

/// Stores common Vulkan-related objects.
struct Common {
  vkb::Instance instance;
  vkb::Device device;
  VkSurfaceKHR surface = nullptr;

  vkb::Swapchain swapchain;
  std::vector<VkImage> swapchain_images;
  std::vector<VkImageView> swapchain_image_views;
};

std::optional<Common> initialize(SDL_Window* window);
void free(Common& vulkan);

}  // namespace Racecar::vk
