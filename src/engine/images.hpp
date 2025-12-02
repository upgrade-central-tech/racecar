#pragma once

#include "../vk/common.hpp"
#include "state.hpp"

#include <filesystem>

enum class FormatType { UNORM8, FLOAT16 };
enum class MIP_TYPE : uint32_t { AUTO_GENERATE = 0 };

namespace racecar::engine {

vk::mem::AllocatedImage create_image( vk::Common& vulkan, engine::State& engine, void* data,
    VkExtent3D size, VkFormat format, VkImageType image_type, VkImageUsageFlags usage_flags,
    bool mipmapped );

vk::mem::AllocatedImage allocate_image( vk::Common& vulkan, VkExtent3D extent, VkFormat format,
    VkImageType image_type, uint32_t mip_levels, uint32_t array_layers,
    VkSampleCountFlagBits samples, VkImageUsageFlags usage_flags, bool mipmapped );

vk::mem::AllocatedImage load_image( std::filesystem::path file_path, vk::Common& vulkan,
    engine::State& engine, size_t desired_channels, VkFormat image_format, bool is_mipmapped );

std::vector<uint16_t> load_image_to_float16( const std::string& global_path );
std::vector<float> load_image_to_float( const std::string& global_path );

} // namespace racecar::engine
