#include "mem.hpp"

namespace racecar::vk::mem {

std::optional<AllocatedBuffer> create_buffer( const Common& vulkan,
                                              size_t alloc_size,
                                              VkBufferUsageFlags usage_flags,
                                              VmaMemoryUsage memory_usage ) {
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = alloc_size,
        .usage = usage_flags,
    };

    // Allocate with flags, determines writability from CPU/GPU, etc.
    // VMA_ALLOCATION_CREATE_MAPPED_BIT allows us to access GPU mem from host
    VmaAllocationCreateInfo vma_alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = memory_usage,
    };

    AllocatedBuffer new_buffer;

    RACECAR_VK_CHECK(
        vmaCreateBuffer( vulkan.allocator, &buffer_info, &vma_alloc_info, &new_buffer.handle,
                         &new_buffer.allocation, &new_buffer.info ),
        "Failed to create GPU buffer" );

    return new_buffer;
}

void free_buffer( const Common& vulkan, const AllocatedBuffer& buffer ) {
    vmaDestroyBuffer( vulkan.allocator, buffer.handle, buffer.allocation );
}

}  // namespace racecar::vk::mem
