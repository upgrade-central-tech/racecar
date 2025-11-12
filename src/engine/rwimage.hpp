#pragma once

#include "../engine/state.hpp"
#include "../vk/mem.hpp"

#include <volk.h>

#include <optional>
#include <vector>

namespace racecar::engine {

/// Abstraction over Vulkan images and takes into consideration the multiple swapchain images.
/// Set of handles. It is trivially copyable.
struct RWImage {
    std::vector<vk::mem::AllocatedImage> images;
};

std::optional<RWImage> create_rwimage( vk::Common& vulkan,
                                       const engine::State& engine,
                                       VkExtent3D extent,
                                       VkFormat format,
                                       VkImageUsageFlags usage_flags,
                                       bool mipmapped );

}  // namespace racecar::engine
