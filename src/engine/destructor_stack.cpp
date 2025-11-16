#include "destructor_stack.hpp"

#include "../log.hpp"
#include "../vk/mem.hpp"

void DestructorStack::execute_cleanup()
{
    while ( !destructors.empty() ) {
        auto destructor = destructors.top();
        destructors.pop();

        destructor();
    }
}

void DestructorStack::push_free_cmd_bufs(
    VkDevice device, VkCommandPool pool, const std::vector<VkCommandBuffer>& buffers )
{
    if ( !buffers.empty() && pool != VK_NULL_HANDLE ) {
        // NOTE: Capturing the vector by value ensures it is available when run() is called later
        destructors.push( [=]() -> void {
            vkFreeCommandBuffers(
                device, pool, static_cast<uint32_t>( buffers.size() ), buffers.data() );
        } );
    }
}

void DestructorStack::push_free_vmabuffer(
    const VmaAllocator allocator, racecar::vk::mem::AllocatedBuffer buffer )
{
    destructors.push(
        [=]() -> void { vmaDestroyBuffer( allocator, buffer.handle, buffer.allocation ); } );
}

void DestructorStack::push_free_vmaimage(
    const VmaAllocator allocator, racecar::vk::mem::AllocatedImage allocated_image )
{
    destructors.push( [=]() -> void {
        racecar::log::info( "Destroyed image {}", static_cast<void*>( allocated_image.image ) );
        vmaDestroyImage( allocator, allocated_image.image, allocated_image.allocation );
    } );
}
