#include "imm_submit.hpp"

#include "../vk/create.hpp"

namespace racecar::engine {

bool immediate_submit( const vk::Common& vulkan, const ImmediateSubmit& immediate_submit,
    std::function<void( VkCommandBuffer command_buffer )>&& function )
{
    RACECAR_VK_CHECK( vkResetFences( vulkan.device, 1, &immediate_submit.fence ),
        "Failed to reset immediate fence" );
    RACECAR_VK_CHECK( vkResetCommandBuffer( immediate_submit.cmd_buf, 0 ),
        "Failed to reset immediate command buffer" );

    VkCommandBufferBeginInfo command_buffer_begin_info
        = vk::create::command_buffer_begin_info( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
    VkCommandBuffer command_buffer = immediate_submit.cmd_buf;

    RACECAR_VK_CHECK( vkBeginCommandBuffer( command_buffer, &command_buffer_begin_info ),
        "Failed to begin command buffer" );

    function( command_buffer );

    RACECAR_VK_CHECK( vkEndCommandBuffer( command_buffer ), "Failed to end command buffer" );

    VkCommandBufferSubmitInfo command_buffer_submit_info
        = vk::create::command_buffer_submit_info( command_buffer );
    VkSubmitInfo2 submit = vk::create::submit_info( &command_buffer_submit_info, nullptr, nullptr );

    RACECAR_VK_CHECK( vkQueueSubmit2( vulkan.graphics_queue, 1, &submit, immediate_submit.fence ),
        "Failed to submit immediate command to the queue" );
    RACECAR_VK_CHECK( vkWaitForFences( vulkan.device, 1, &immediate_submit.fence, true,
                          std::numeric_limits<uint64_t>::max() ),
        "Failed to wait for immediate fence following queue submit" );

    return true;
};

void create_immediate_commands( ImmediateSubmit& immediate_submit, vk::Common& vulkan )
{
    VkCommandPoolCreateInfo command_pool_info = vk::create::command_pool_info(
        vulkan.graphics_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT );
    vk::check( vkCreateCommandPool(
                   vulkan.device, &command_pool_info, nullptr, &immediate_submit.cmd_pool ),
        "Failed to create immediate command pool" );

    VkCommandBufferAllocateInfo command_buffer_allocate_info
        = vk::create::command_buffer_allocate_info( immediate_submit.cmd_pool, 1 );
    vk::check( vkAllocateCommandBuffers(
                   vulkan.device, &command_buffer_allocate_info, &immediate_submit.cmd_buf ),
        "Failed to allocate immediate command buffer" );

    vulkan.destructor_stack.push_free_cmd_bufs(
        vulkan.device, immediate_submit.cmd_pool, { immediate_submit.cmd_buf } );
    vulkan.destructor_stack.push( vulkan.device, immediate_submit.cmd_pool, vkDestroyCommandPool );
};

void create_immediate_sync_structures( ImmediateSubmit& immediate_submit, vk::Common& vulkan )
{
    VkFenceCreateInfo fence_info = vk::create::fence_info( VK_FENCE_CREATE_SIGNALED_BIT );

    vk::check( vkCreateFence( vulkan.device, &fence_info, nullptr, &immediate_submit.fence ),
        "Failed to create immediate fence" );

    vulkan.destructor_stack.push( vulkan.device, immediate_submit.fence, vkDestroyFence );
};

} // namespace racecar::engine
