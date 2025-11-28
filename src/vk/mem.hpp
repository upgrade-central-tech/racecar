#pragma once

#include "common.hpp"
#include "vma.hpp"

namespace racecar::vk::mem {

struct AllocatedBuffer {
    VkBuffer handle = nullptr;
    VmaAllocation allocation = nullptr;
    VmaAllocationInfo info = {};
};

struct AllocatedImage {
    VkImage image = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    VkImageView storage_image_view = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkExtent3D image_extent = {};
    VkFormat image_format = VK_FORMAT_UNDEFINED;
};

struct UniformBuffer {
    AllocatedBuffer buffer;
    size_t data_size = 0;
    void* mapped_data = nullptr;
};

AllocatedBuffer create_buffer( Common& vulkan, size_t alloc_size, VkBufferUsageFlags usage_flags,
    VmaMemoryUsage memory_usage );

} // namespace racecar::vk::mem
