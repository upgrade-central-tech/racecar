#pragma once

#include "common.hpp"
#include "vma.hpp"

#include <optional>

namespace racecar::vk::mem {

struct AllocatedBuffer {
    VkBuffer handle = nullptr;
    VmaAllocation allocation = nullptr;
    VmaAllocationInfo info = {};
};

struct AllocatedImage {
    VkImage image = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkExtent3D image_extent = {};
    VkFormat image_format = VK_FORMAT_UNDEFINED;
};

struct DebugImageData {
    std::optional<vk::mem::AllocatedImage> white_image;
    std::optional<vk::mem::AllocatedImage> checkerboard_image;

    VkSampler default_sampler_linear;
};

struct UniformBuffer {
    AllocatedBuffer buffer;
    size_t data_size = 0;
    void* mapped_data = nullptr;
};

std::optional<UniformBuffer> create_uniform_buffer( Common& vulkan,
                                                    const void* uniform_data,
                                                    size_t data_size,
                                                    uint32_t binding_slot );

std::optional<mem::AllocatedBuffer> create_buffer( Common& vulkan,
                                                   size_t alloc_size,
                                                   VkBufferUsageFlags usage_flags,
                                                   VmaMemoryUsage memory_usage );

}  // namespace racecar::vk::mem
