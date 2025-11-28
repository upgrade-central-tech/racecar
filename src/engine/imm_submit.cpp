#include "imm_submit.hpp"

#include "../log.hpp"
#include "../vk/create.hpp"

namespace racecar::engine {

void immediate_submit( const vk::Common& vulkan, const ImmediateSubmit& immediate_submit,
    std::function<void( VkCommandBuffer command_buffer )>&& function )
{
    vk::check( vkResetFences( vulkan.device, 1, &immediate_submit.fence ),
        "Failed to reset immediate fence" );
    vk::check( vkResetCommandBuffer( immediate_submit.cmd_buf, 0 ),
        "Failed to reset immediate command buffer" );

    VkCommandBufferBeginInfo command_buffer_begin_info
        = vk::create::command_buffer_begin_info( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
    VkCommandBuffer command_buffer = immediate_submit.cmd_buf;

    vk::check( vkBeginCommandBuffer( command_buffer, &command_buffer_begin_info ),
        "Failed to begin command buffer" );

    function( command_buffer );

    vk::check( vkEndCommandBuffer( command_buffer ), "Failed to end command buffer" );

    VkCommandBufferSubmitInfo command_buffer_submit_info
        = vk::create::command_buffer_submit_info( command_buffer );
    VkSubmitInfo2 submit = vk::create::submit_info( &command_buffer_submit_info, nullptr, nullptr );

    vk::check( vkQueueSubmit2( vulkan.graphics_queue, 1, &submit, immediate_submit.fence ),
        "Failed to submit immediate command to the queue" );
    vk::check( vkWaitForFences( vulkan.device, 1, &immediate_submit.fence, true,
                   std::numeric_limits<uint64_t>::max() ),
        "Failed to wait for immediate fence following queue submit" );
};

void create_immediate_commands( ImmediateSubmit& immediate_submit, vk::Common& vulkan )
{
    VkCommandPoolCreateInfo command_pool_info = vk::create::command_pool_info(
        vulkan.graphics_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT );
    vk::check( vkCreateCommandPool(
                   vulkan.device, &command_pool_info, nullptr, &immediate_submit.cmd_pool ),
        "Failed to create immediate command pool" );
    vulkan.destructor_stack.push( vulkan.device, immediate_submit.cmd_pool, vkDestroyCommandPool );

    log::info( "[engine] Created immediate command pool {}",
        static_cast<void*>( immediate_submit.cmd_pool ) );

    VkCommandBufferAllocateInfo command_buffer_allocate_info
        = vk::create::command_buffer_allocate_info( immediate_submit.cmd_pool, 1 );
    vk::check( vkAllocateCommandBuffers(
                   vulkan.device, &command_buffer_allocate_info, &immediate_submit.cmd_buf ),
        "Failed to allocate immediate command buffer" );
    vulkan.destructor_stack.push_free_cmd_bufs(
        vulkan.device, immediate_submit.cmd_pool, { immediate_submit.cmd_buf } );

    log::info( "[engine] Created immediate command buffer {}",
        static_cast<void*>( immediate_submit.cmd_buf ) );
};

void create_immediate_sync_structures( ImmediateSubmit& immediate_submit, vk::Common& vulkan )
{
    VkFenceCreateInfo fence_info = vk::create::fence_info( VK_FENCE_CREATE_SIGNALED_BIT );

    vk::check( vkCreateFence( vulkan.device, &fence_info, nullptr, &immediate_submit.fence ),
        "Failed to create immediate fence" );

    vulkan.destructor_stack.push( vulkan.device, immediate_submit.fence, vkDestroyFence );
};

} // namespace racecar::engine
