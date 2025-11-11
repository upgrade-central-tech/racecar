#include "mem.hpp"

#include "create.hpp"
#include "utility.hpp"

namespace racecar::vk::mem {

std::optional<UniformBuffer> create_uniform_buffer( Common& vulkan, size_t data_size ) {
    UniformBuffer uniform_buffer = {
        // uniform_data can potentially be out of scope.
        // Shouldn't be a problem at all if we keep everything in main's scope (ideally no heap mem,
        // then).
        .data_size = data_size,
    };

    std::optional<AllocatedBuffer> allocated_buffer = create_buffer(
        vulkan, data_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU );

    if ( !allocated_buffer ) {
        SDL_Log( "[Vulkan] Failed to create uniform buffer" );
        return {};
    }

    uniform_buffer.buffer = allocated_buffer.value();

    RACECAR_VK_CHECK( vmaMapMemory( vulkan.allocator, uniform_buffer.buffer.allocation,
                                    &uniform_buffer.mapped_data ),
                      "Failed to set up map pointer to GPU uniform buffer data" );

    return uniform_buffer;
};

template <typename T>
bool update_uniform_buffer( UniformBuffer& uniform_buffer, const T& updated_buffer_data ) {
    memcpy( uniform_buffer.mapped_data, &updated_buffer_data, sizeof( T ) );
}

std::optional<AllocatedBuffer> create_buffer( Common& vulkan,
                                              size_t alloc_size,
                                              VkBufferUsageFlags usage_flags,
                                              VmaMemoryUsage memory_usage ) {
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

    RACECAR_VK_CHECK(
        vmaCreateBuffer( vulkan.allocator, &buffer_info, &vma_alloc_info, &new_buffer.handle,
                         &new_buffer.allocation, &new_buffer.info ),
        "Failed to create GPU buffer" );

    vulkan.destructor_stack.push_free_vmabuffer( vulkan.allocator, new_buffer );

    return new_buffer;
}

}  // namespace racecar::vk::mem
