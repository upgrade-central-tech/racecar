#include "mem.hpp"

namespace racecar::vk::mem {

AllocatedBuffer create_buffer(
    Common& vulkan, size_t alloc_size, VkBufferUsageFlags usage_flags, VmaMemoryUsage memory_usage )
{
    // We may want to adjust the sharingMode to be adjustable depending on use-case.
    // As long as buffers are separated between graphics and compute queues,
    // VK_SHARING_MODE_EXCLUSIVE is fine.
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = alloc_size,
        .usage = usage_flags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    // Allocate with flags, determines writability from CPU/GPU, etc.
    // VMA_ALLOCATION_CREATE_MAPPED_BIT allows us to access GPU mem from host
    VmaAllocationCreateInfo vma_alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = memory_usage,
    };

    AllocatedBuffer new_buffer;

    vk::check( vmaCreateBuffer( vulkan.allocator, &buffer_info, &vma_alloc_info, &new_buffer.handle,
                   &new_buffer.allocation, &new_buffer.info ),
        "[VMA] Failed to create GPU buffer" );
    vulkan.destructor_stack.push_free_vmabuffer( vulkan.allocator, new_buffer );

    return new_buffer;
}

} // namespace racecar::vk::mem
