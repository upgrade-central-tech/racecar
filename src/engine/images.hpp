#pragma once

#include "../vk/common.hpp"
#include "state.hpp"

namespace racecar::engine {

vk::mem::AllocatedImage create_image( vk::Common& vulkan, engine::State& engine, void* data,
    VkExtent3D size, VkFormat format, VkImageType image_type, VkImageUsageFlags usage,
    bool mipmapped );

vk::mem::AllocatedImage allocate_image( vk::Common& vulkan, VkExtent3D extent, VkFormat format,
    VkImageType image_type, VkImageUsageFlags usage, bool mipmapped );

} // namespace racecar::engine
