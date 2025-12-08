#pragma once

#include "../engine/state.hpp"
#include "../vk/mem.hpp"

#include <volk.h>

#include <vector>

namespace racecar::engine {

/// Abstraction over Vulkan images and takes into consideration the multiple swapchain images.
/// Set of handles. It is trivially copyable.
struct RWImage {
    std::vector<vk::mem::AllocatedImage> images;
};

RWImage create_rwimage( vk::Common& vulkan, const engine::State& engine, VkExtent3D extent,
    VkFormat format, VkImageType image_type, VkSampleCountFlagBits samples,
    VkImageUsageFlags usage_flags );

RWImage create_rwimage_mips( vk::Common& vulkan, const engine::State& engine, VkExtent3D extent,
    VkFormat format, VkImageType image_type, VkSampleCountFlagBits samples,
    VkImageUsageFlags usage_flags, uint32_t mip_levels );

} // namespace racecar::engine
