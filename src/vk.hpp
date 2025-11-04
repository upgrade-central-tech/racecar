#pragma once

#include <SDL3/SDL.h>
#include <volk.h>
#include <VkBootstrap.h>

#include <optional>

/// Custom define based on VK_CHECK, just with our SDL_Log. Feel free to tweak this.
#define RACECAR_VK_CHECK(x, msg)                         \
  do {                                                   \
    VkResult err = x;                                    \
    if (err) {                                           \
      SDL_Log("[Vulkan] %s | Error code: %d", msg, err); \
      return {};                                         \
    }                                                    \
  } while (0)

namespace racecar::vk {

/// Frame objects per view in the swapchain.
struct FrameData {
  VkCommandPool command_pool;
  VkCommandBuffer command_buffer;

  /// Synchronization objects. Use sempahores for GPU/GPU sync, fences for CPU/GPU sync,
  VkSemaphore swapchain_semaphore, render_semaphore;
  VkFence render_fence;
};

/// Stores common Vulkan-related objects.
struct Common {
  vkb::Instance instance;
  vkb::Device device;
  VkSurfaceKHR surface = nullptr;

  vkb::Swapchain swapchain;
  std::vector<VkImage> swapchain_images;
  std::vector<VkImageView> swapchain_image_views;

  uint32_t graphics_queue_family;
  VkQueue graphics_queue;

  std::vector<FrameData> frames;
  uint32_t frame_overlap;
  uint32_t frame_number;
};

std::optional<Common> initialize(SDL_Window* window);

void initialize_commands();

void free(Common& vulkan);

}  // namespace racecar::vk
