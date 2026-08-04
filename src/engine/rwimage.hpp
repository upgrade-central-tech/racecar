#pragma once

#include "../engine/state.hpp"
#include "../vk/mem.hpp"

#include <volk.h>

#include <vector>

namespace racecar::engine {

/// Abstraction over Vulkan images and takes into consideration the multiple swapchain images.
///
/// Move-only
/// An RWImage is the single owner of its per-frame images
struct RWImage {
    std::vector<vk::mem::AllocatedImage> images;

    RWImage() = default;

    RWImage( const RWImage& ) = delete;
    RWImage& operator=( const RWImage& ) = delete;

    RWImage( RWImage&& ) = default;
    RWImage& operator=( RWImage&& ) = default;
};

RWImage create_rwimage( vk::Common& vulkan, const engine::State& engine, VkExtent3D extent,
    VkFormat format, VkImageType image_type, VkSampleCountFlagBits samples,
    VkImageUsageFlags usage_flags );

RWImage create_rwimage_mips( vk::Common& vulkan, const engine::State& engine, VkExtent3D extent,
    VkFormat format, VkImageType image_type, VkSampleCountFlagBits samples,
    VkImageUsageFlags usage_flags, uint32_t mip_levels );

RWImage create_gbuffer_image( vk::Common& vulkan, const engine::State& engine, VkFormat format,
    VkSampleCountFlagBits samples );

/// Transitions every frame's image out of UNDEFINED once, immediately.
/// For images whose contents must survive across frames (AA)
void initialize_rwimage_layout(
    vk::Common& vulkan, engine::State& engine, RWImage& image, VkImageLayout layout );

} // namespace racecar::engine
