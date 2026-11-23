#pragma once

#include "../engine/state.hpp"
#include "../vk/mem.hpp"

#include <volk.h>

#include <filesystem>

namespace racecar::engine {

/// Abstraction over a single Vulkan image.
///
/// Move-only
/// An RWImage is the single owner of its image
struct RWImage {
    vk::mem::AllocatedImage image;

    RWImage() = default;

    RWImage( const RWImage& ) = delete;
    RWImage& operator=( const RWImage& ) = delete;

    RWImage( RWImage&& ) = default;
    RWImage& operator=( RWImage&& ) = default;
};

RWImage create_rwimage( VkExtent3D extent,
    VkFormat format, VkImageType image_type, VkSampleCountFlagBits samples,
    VkImageUsageFlags usage_flags );

RWImage create_rwimage_mips( VkExtent3D extent,
    VkFormat format, VkImageType image_type, VkSampleCountFlagBits samples,
    VkImageUsageFlags usage_flags, uint32_t mip_levels );

RWImage create_gbuffer_image( VkFormat format,
    VkSampleCountFlagBits samples );

RWImage create_rwimage_from_data( void* data, VkExtent3D extent, VkFormat format,
    VkImageType image_type, VkImageUsageFlags usage_flags, bool mipmapped );

RWImage create_rwimage_from_load( std::filesystem::path file_path, size_t desired_channels,
    VkFormat image_format, bool is_mipmapped );

/// Transitions the image out of UNDEFINED once, immediately.
/// For images whose contents must survive across frames (AA)
void initialize_rwimage_layout(
    RWImage& image, VkImageLayout layout );

} // namespace racecar::engine
