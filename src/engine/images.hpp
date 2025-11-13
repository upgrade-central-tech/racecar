#pragma once

#include "../vk/common.hpp"
#include "state.hpp"

#include <optional>

namespace racecar::engine {

std::optional<vk::mem::AllocatedImage> create_image( vk::Common& vulkan, engine::State& engine,
    void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped );

std::optional<vk::mem::AllocatedImage> allocate_image( vk::Common& vulkan, VkExtent3D extent,
    VkFormat format, VkImageUsageFlags usage, bool mipmapped );

std::optional<vk::mem::AllocatedImage> create_allocated_image( vk::Common& vulkan,
    engine::State& engine, void* data, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage,
    bool mipmapped );

void destroy_image( vk::Common& vulkan, const vk::mem::AllocatedImage& image );

} // namespace racecar::engine
