#pragma once

#include "../engine/state.hpp"
#include "../vk/mem.hpp"
#include "../vk/vma.hpp"

#include <volk.h>

#include <filesystem>
#include <vector>

namespace racecar::engine {

/// Abstraction over a single Vulkan image.
///
/// Move-only
/// An Image is the single owner of its Vulkan image, views, and allocation
struct Image {
    VkImage vk_image = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkExtent3D image_extent = { };
    VkFormat image_format = VK_FORMAT_UNDEFINED;

    std::vector<VkImageView> mip_levels;

    Image() = default;

    Image( const Image& ) = delete;
    Image& operator=( const Image& ) = delete;

    Image( Image&& ) = default;
    Image& operator=( Image&& ) = default;
};

Image create_image( VkExtent3D extent, VkFormat format, VkImageType image_type,
    VkSampleCountFlagBits samples, VkImageUsageFlags usage_flags );

Image create_image_mips( VkExtent3D extent, VkFormat format, VkImageType image_type,
    VkSampleCountFlagBits samples, VkImageUsageFlags usage_flags, uint32_t mip_levels );

Image create_gbuffer_image( VkFormat format, VkSampleCountFlagBits samples );

Image create_image_from_data( void* data, VkExtent3D extent, VkFormat format,
    VkImageType image_type, VkImageUsageFlags usage_flags, bool mipmapped );

Image create_image_from_load( std::filesystem::path file_path, size_t desired_channels,
    VkFormat image_format, bool is_mipmapped );

/// Transitions the image out of UNDEFINED once, immediately.
/// For images whose contents must survive across frames (AA)
void initialize_image_layout( Image& image, VkImageLayout layout );

} // namespace racecar::engine
