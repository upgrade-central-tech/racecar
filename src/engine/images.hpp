#pragma once

#include "../vk/common.hpp"
#include "state.hpp"
#include <filesystem>

namespace racecar::engine {

vk::mem::AllocatedImage create_image( vk::Common& vulkan, engine::State& engine, void* data,
    VkExtent3D size, VkFormat format, VkImageType image_type, VkImageUsageFlags usage_flags,
    bool mipmapped );

vk::mem::AllocatedImage allocate_image( vk::Common& vulkan, VkExtent3D extent, VkFormat format,
    VkImageType image_type, uint32_t mip_levels, uint32_t array_layers,
    VkImageUsageFlags usage_flags, bool mipmapped );

vk::mem::AllocatedImage allocate_ms_image( vk::Common& vulkan, VkExtent3D extent, VkFormat format,
    VkImageType image_type, uint32_t mip_levels, uint32_t array_layers,
    VkSampleCountFlagBits samples, VkImageUsageFlags usage_flags, bool mipmapped );

vk::mem::AllocatedImage load_image( std::filesystem::path file_path, vk::Common& vulkan, engine::State& engine, int chnanels, VkFormat image_format );
std::vector<uint16_t> load_image_to_float16( const std::string& global_path );
std::vector<float> load_image_to_float( const std::string& global_path );

} // namespace racecar::engine
