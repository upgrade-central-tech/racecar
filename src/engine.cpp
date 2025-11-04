#include "engine.hpp"

#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <limits>

namespace Racecar {

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
    SDL_Log("[SDL] Enabling necessary Vulkan instance extensions:");

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
std::optional<vkb::Device> pick_and_create_vulkan_device(const Engine& engine) {
  vkb::PhysicalDeviceSelector phys_selector(engine.instance, engine.surface);
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

  SDL_Log("[Engine] Selected physical device \"%s\"", phys_name.c_str());

  return device;
}

std::optional<vkb::Swapchain> create_swapchain(const SDL::Context& ctx, const Engine& engine) {
  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(engine.device.physical_device, engine.surface,
                                            &capabilities);

  VkExtent2D swap_extent = {.width = 0, .height = 0};
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    swap_extent = capabilities.currentExtent;
  } else {
    int width = 0;
    int height = 0;

    if (!SDL_GetWindowSizeInPixels(ctx.window, &width, &height)) {
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

  SDL_Log("[Engine] Initial desired swapchain extent: %d×%d", swap_extent.width,
          swap_extent.height);

  vkb::SwapchainBuilder swapchain_builder(engine.device);
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

}  // namespace

std::optional<Engine> initialize_engine(const SDL::Context& ctx) {
  Engine engine;

  if (std::optional<vkb::Instance> instance_opt = create_vulkan_instance(); !instance_opt) {
    SDL_Log("[Engine] Failed to create Vulkan instance");
    return {};
  } else {
    engine.instance = instance_opt.value();
  }

  volkLoadInstance(engine.instance);

  if (!SDL_Vulkan_CreateSurface(ctx.window, engine.instance, nullptr, &engine.surface)) {
    SDL_Log("[SDL] Could not create Vulkan surface: %s", SDL_GetError());
    return {};
  }

  if (std::optional<vkb::Device> device_opt = pick_and_create_vulkan_device(engine); !device_opt) {
    SDL_Log("[Engine] Failed to create Vulkan device");
    return {};
  } else {
    engine.device = device_opt.value();
  }

  if (std::optional<vkb::Swapchain> swapchain_opt = create_swapchain(ctx, engine); !swapchain_opt) {
    SDL_Log("[Engine] Failed to create swapchain");
    return {};
  } else {
    engine.swapchain = swapchain_opt.value();
  }

  if (vkb::Result<std::vector<VkImage>> images_res = engine.swapchain.get_images(); !images_res) {
    SDL_Log("[vkb] Failed to get swapchain images: %s", images_res.error().message().c_str());
    return {};
  } else {
    engine.swapchain_images = std::move(images_res.value());
  }

  if (vkb::Result<std::vector<VkImageView>> image_views_res = engine.swapchain.get_image_views();
      !image_views_res) {
    SDL_Log("[vkb] Failed to get swapchain image views: %s",
            image_views_res.error().message().c_str());
    return {};
  } else {
    engine.swapchain_image_views = std::move(image_views_res.value());
  }

  return engine;
}

void draw(const SDL::Context&) {}

void clean_up(Engine& engine) {
  engine.swapchain.destroy_image_views(engine.swapchain_image_views);
  vkb::destroy_swapchain(engine.swapchain);
  vkb::destroy_device(engine.device);
  SDL_Vulkan_DestroySurface(engine.instance, engine.surface, nullptr);
  vkb::destroy_instance(engine.instance);

  engine = {
      .instance = {},
      .device = {},
      .surface = nullptr,

      .swapchain = {},
      .swapchain_images = {},
      .swapchain_image_views = {},
  };
}

}  // namespace Racecar
