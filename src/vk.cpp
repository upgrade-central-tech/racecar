#include "vk.hpp"

#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <limits>

namespace racecar::vk {

namespace {

std::optional<vkb::Instance> create_vulkan_instance() {
  vkb::Result<vkb::SystemInfo> system_info_ret = vkb::SystemInfo::get_system_info();

  if (!system_info_ret) {
    SDL_Log("[vkb] Unable to get system info");
    return {};
  }

  vkb::SystemInfo& system_info = system_info_ret.value();

  if (auto handler =
          reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());
      !handler) {
    SDL_Log("[SDL] Could not get vkGetInstanceProcAddr: %s", SDL_GetError());
    return {};
  } else {
    volkInitializeCustom(handler);
  }

  vkb::InstanceBuilder instance_builder;
  instance_builder.set_app_name("RACECAR").set_app_version(1, 0, 0).require_api_version(1, 4);

#if DEBUG
  if (system_info.validation_layers_available) {
    instance_builder.enable_validation_layers().use_default_debug_messenger();
  }
#endif

  uint32_t extension_count = 0;
  if (const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
      !extensions) {
    SDL_Log("[SDL] Could not get necessary Vulkan instance extensions: %s", SDL_GetError());
    return {};
  } else {
    SDL_Log("[Vulkan] Enabling necessary instance extensions:");

    for (uint32_t i = 0; i < extension_count; ++i) {
      const char* extension = extensions[i];
      SDL_Log("- %s", extension);

      if (system_info.is_extension_available(extension)) {
        instance_builder.enable_extension(extension);
      } else {
        SDL_Log("[vkb] Necessary Vulkan instance extension not available: %s", extension);
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

  return instance_ret.value();
}

/// Picks a physical device with a preference for a discrete GPU, and then creates a logical
/// device with one queue enabled from each available queue family.
std::optional<vkb::Device> pick_and_create_vulkan_device(const Common& vulkan) {
  vkb::PhysicalDeviceSelector phys_selector(vulkan.instance, vulkan.surface);
  vkb::Result<vkb::PhysicalDevice> phys_selector_ret =
      phys_selector.prefer_gpu_device_type()
          .add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)
          .select();

  if (!phys_selector_ret) {
    auto phys_device_error =
        static_cast<vkb::PhysicalDeviceError>(phys_selector_ret.error().value());

    // Handle no physical devices separately
    switch (phys_device_error) {
      case vkb::PhysicalDeviceError::no_physical_devices_found: {
        SDL_Log("[vkb] No physical devices found");
        return {};
      }

      default: {
        SDL_Log("[vkb] Physical device selection failed because: %s",
                vkb::to_string(phys_device_error));
        return {};
      }
    }
  }

  vkb::PhysicalDevice& phys_device = phys_selector_ret.value();
  const std::string& phys_name = phys_device.name;
  vkb::DeviceBuilder device_builder(phys_device);
  vkb::Result<vkb::Device> device_ret = device_builder.build();

  if (!device_ret) {
    SDL_Log("[vkb] Failed to create VkDevice from VkPhysicalDevice named \"%s\"",
            phys_name.c_str());
    return {};
  }

  vkb::Device& device = device_ret.value();

  if (!device.get_queue(vkb::QueueType::graphics)) {
    SDL_Log("[vkb] VkDevice created from physical device \"%s\" does not have a graphics queue!",
            phys_name.c_str());
    return {};
  }

  if (!device.get_queue(vkb::QueueType::present)) {
    SDL_Log("[vkb] VkDevice created from physical device \"%s\" does not have a present queue!",
            phys_name.c_str());
    return {};
  }

  SDL_Log("[Vulkan] Selected physical device \"%s\"", phys_name.c_str());

  return device;
}

std::optional<vkb::Swapchain> create_swapchain(SDL_Window* window, const Common& vulkan) {
  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkan.device.physical_device, vulkan.surface,
                                            &capabilities);

  VkExtent2D swap_extent = {.width = 0, .height = 0};
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    swap_extent = capabilities.currentExtent;
  } else {
    int width = 0;
    int height = 0;

    if (!SDL_GetWindowSizeInPixels(window, &width, &height)) {
      SDL_Log("[SDL] SDL_GetWindowSizeInPixels: %s", SDL_GetError());
      return {};
    }

    swap_extent = {
        .width = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width,
                            capabilities.maxImageExtent.width),
        .height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height,
                             capabilities.maxImageExtent.height),
    };
  }

  SDL_Log("[Vulkan] Initial desired swapchain extent: %d×%d", swap_extent.width,
          swap_extent.height);

  vkb::SwapchainBuilder swapchain_builder(vulkan.device);
  vkb::Result<vkb::Swapchain> swapchain_ret =
      swapchain_builder.set_desired_extent(swap_extent.width, swap_extent.height)
          .set_desired_min_image_count(capabilities.minImageCount + 1)
          .build();

  if (!swapchain_ret) {
    auto swapchain_error = static_cast<vkb::SwapchainError>(swapchain_ret.error().value());
    SDL_Log("[vkb] Swapchain creation failed: %s", vkb::to_string(swapchain_error));
    return {};
  }

  return swapchain_ret.value();
}

VkCommandPoolCreateInfo command_pool_create_info(uint32_t queue_family_index,
                                                 VkCommandPoolCreateFlags flags) {
  VkCommandPoolCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  info.pNext = nullptr;
  info.queueFamilyIndex = queue_family_index;
  info.flags = flags;

  return info;
}

VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, uint32_t count) {
  VkCommandBufferAllocateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  info.pNext = nullptr;

  info.commandPool = pool;
  info.commandBufferCount = count;
  info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

  return info;
}

VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags) {
  VkFenceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = flags;

  return info;
}

VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags) {
  VkSemaphoreCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = flags;

  return info;
}

}  // namespace

std::optional<Common> initialize(SDL_Window* window) {
  Common vulkan;

  if (std::optional<vkb::Instance> instance_opt = create_vulkan_instance(); !instance_opt) {
    SDL_Log("[Vulkan] Failed to create Vulkan instance");
    return {};
  } else {
    vulkan.instance = instance_opt.value();
  }

  volkLoadInstance(vulkan.instance);

  if (!SDL_Vulkan_CreateSurface(window, vulkan.instance, nullptr, &vulkan.surface)) {
    SDL_Log("[SDL] Could not create Vulkan surface: %s", SDL_GetError());
    return {};
  }

  if (std::optional<vkb::Device> device_opt = pick_and_create_vulkan_device(vulkan); !device_opt) {
    SDL_Log("[Vulkan] Failed to create Vulkan device");
    return {};
  } else {
    vulkan.device = device_opt.value();
  }

  if (std::optional<vkb::Swapchain> swapchain_opt = create_swapchain(window, vulkan);
      !swapchain_opt) {
    SDL_Log("[Vulkan] Failed to create swapchain");
    return {};
  } else {
    vulkan.swapchain = swapchain_opt.value();
  }

  if (vkb::Result<std::vector<VkImage>> images_res = vulkan.swapchain.get_images(); !images_res) {
    SDL_Log("[vkb] Failed to get swapchain images: %s", images_res.error().message().c_str());
    return {};
  } else {
    vulkan.swapchain_images = std::move(images_res.value());
  }

  if (vkb::Result<std::vector<VkImageView>> image_views_res = vulkan.swapchain.get_image_views();
      !image_views_res) {
    SDL_Log("[vkb] Failed to get swapchain image views: %s",
            image_views_res.error().message().c_str());
    return {};
  } else {
    vulkan.swapchain_image_views = std::move(image_views_res.value());
  }

  if (vkb::Result<VkQueue> graphics_queue =
          vulkan.device.get_queue(vkb::QueueType::graphics).value();
      !graphics_queue) {
    SDL_Log("[vkb] Failed to get graphics queue: %s", graphics_queue.error().message().c_str());
    return {};
  } else {
    vulkan.graphics_queue = std::move(graphics_queue.value());
  }

  if (vkb::Result<uint32_t> graphics_queue_family =
          vulkan.device.get_queue_index(vkb::QueueType::graphics).value();
      !graphics_queue_family) {
    SDL_Log("[vkb] Failed to get graphics queue family: %s",
            graphics_queue_family.error().message().c_str());
    return {};
  } else {
    vulkan.graphics_queue_family = graphics_queue_family.value();
  }

  // Graphics command pool/frame setup.
  // Similar to the create sync reources below, this scope should be abstracted away.
  {
    vulkan.frames = std::vector<FrameData>(vulkan.swapchain_images.size());
    vulkan.frame_overlap = static_cast<uint32_t>(vulkan.swapchain_images.size());
    vulkan.frame_number = 0;

    VkCommandPoolCreateInfo graphics_command_pool_info = command_pool_create_info(
        vulkan.graphics_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (uint32_t i = 0; i < vulkan.frame_overlap; i++) {
      FrameData* frame = &vulkan.frames[i];

      RACECAR_VK_CHECK(vkCreateCommandPool(vulkan.device, &graphics_command_pool_info, nullptr,
                                           &frame->command_pool),
                       "Failed to create command pool");

      VkCommandBufferAllocateInfo cmd_buffer_allocate_info =
          command_buffer_allocate_info(frame->command_pool, 1);

      RACECAR_VK_CHECK(vkAllocateCommandBuffers(vulkan.device, &cmd_buffer_allocate_info,
                                                &frame->command_buffer),
                       "Failed to allocate command buffer");
    }
  }

  // Create sync structures - I'll leave this open for review. This should be abstraced away.
  {
    // Flag makes the fence signaled first to trigger a wait.
    VkFenceCreateInfo fence_create = fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphore_create = semaphore_create_info(0);

    for (uint32_t i = 0; i < vulkan.frame_overlap; i++) {
      FrameData* frame = &vulkan.frames[i];

      RACECAR_VK_CHECK(vkCreateFence(vulkan.device, &fence_create, nullptr, &frame->render_fence),
                       "Failed to create render fence");
      RACECAR_VK_CHECK(
          vkCreateSemaphore(vulkan.device, &semaphore_create, nullptr, &frame->swapchain_semaphore),
          "Failed to create swapchain semaphore");
      RACECAR_VK_CHECK(
          vkCreateSemaphore(vulkan.device, &semaphore_create, nullptr, &frame->render_semaphore),
          "Failed to create render semaphore");
    }
  }

  return vulkan;
}

void initialize_commands() {}

void free(Common& vulkan) {
  // Kill per-frame resources from vulkan.frames.
  for (uint32_t i = 0; i < vulkan.frame_overlap; i++) {
    // Kill command resources.
    vkDestroyCommandPool(vulkan.device, vulkan.frames[i].command_pool, nullptr);

    // Kill sync resources.
    vkDestroyFence(vulkan.device, vulkan.frames[i].render_fence, nullptr);
    vkDestroySemaphore(vulkan.device, vulkan.frames[i].render_semaphore, nullptr);
    vkDestroySemaphore(vulkan.device, vulkan.frames[i].swapchain_semaphore, nullptr);
  }

  vulkan.swapchain.destroy_image_views(vulkan.swapchain_image_views);
  vkb::destroy_swapchain(vulkan.swapchain);
  vkb::destroy_device(vulkan.device);
  SDL_Vulkan_DestroySurface(vulkan.instance, vulkan.surface, nullptr);
  vkb::destroy_instance(vulkan.instance);

  vulkan = {};
}

}  // namespace racecar::vk
