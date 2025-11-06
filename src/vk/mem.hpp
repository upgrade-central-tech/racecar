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

/// Memory creation.
std::optional<AllocatedBuffer> create_buffer( const Common& vulkan,
                                              size_t alloc_size,
                                              VkBufferUsageFlags usage_flags,
                                              VmaMemoryUsage memory_usage );

void free_buffer( const Common& vulkan, const AllocatedBuffer& buffer );

}  // namespace racecar::vk::mem
