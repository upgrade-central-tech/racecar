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

struct UniformBuffer {
    AllocatedBuffer buffer;
    size_t data_size = 0;
    void* mapped_data = nullptr;
};

std::optional<UniformBuffer> create_uniform_buffer( Common& vulkan,
                                                    const void* uniform_data,
                                                    size_t data_size,
                                                    uint32_t binding_slot );

template <typename T>
bool update_uniform_buffer( UniformBuffer& uniform_buffer, const T& updated_buffer_data );

/// Memory creation.
std::optional<AllocatedBuffer> create_buffer( Common& vulkan,
                                              size_t alloc_size,
                                              VkBufferUsageFlags usage_flags,
                                              VmaMemoryUsage memory_usage );

}  // namespace racecar::vk::mem
