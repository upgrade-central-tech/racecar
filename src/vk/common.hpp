#pragma once

#include <SDL3/SDL.h>
#include <volk.h>
#include <VkBootstrap.h>

#include <optional>

/// Custom define based on VK_CHECK, just with our SDL_Log. Feel free to tweak this.
#define RACECAR_VK_CHECK(vk_fn, message)                        \
  do {                                                          \
    VkResult result = vk_fn;                                    \
    if (result) {                                               \
      SDL_Log("[Vulkan] %s | Error code: %d", message, result); \
      return {};                                                \
    }                                                           \
  } while (0)

namespace racecar::vk {

/// Contains Vulkan objects for each image view in the swapchain.
struct FrameData {
  VkCommandPool command_pool = nullptr;
  VkCommandBuffer command_buffer = nullptr;
  VkSemaphore swapchain_semaphore = nullptr;
  VkFence render_fence = nullptr;
};

/// Stores common Vulkan-related objects.
struct Common {
  vkb::Instance instance;
  vkb::Device device;
  VkSurfaceKHR surface = nullptr;

  vkb::Swapchain swapchain;
  std::vector<VkImage> swapchain_images;
  std::vector<VkImageView> swapchain_image_views;

  uint32_t graphics_queue_family = 0;
  VkQueue graphics_queue;

  std::vector<FrameData> frames;
  std::vector<VkSemaphore> render_semaphores;
  uint32_t frame_overlap = 0;
  uint32_t frame_number = 0;
  uint32_t rendered_frames = 0;
};

std::optional<Common> initialize(SDL_Window* window);
void free(Common& vulkan);

}  // namespace racecar::vk
