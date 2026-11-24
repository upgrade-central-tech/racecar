#pragma once

#include "common.hpp"
#include "vma.hpp"

namespace racecar::vk::mem {

struct AllocatedBuffer {
    VkBuffer handle = nullptr;
    VmaAllocation allocation = nullptr;
    VmaAllocationInfo info = {};
};

struct UniformBuffer {
    AllocatedBuffer buffer;
    size_t data_size = 0;
    void* mapped_data = nullptr;
};

AllocatedBuffer create_buffer( size_t alloc_size, VkBufferUsageFlags usage_flags,
    VmaMemoryUsage memory_usage );

} // namespace racecar::vk::mem
