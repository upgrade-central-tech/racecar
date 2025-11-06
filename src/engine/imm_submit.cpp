#include "imm_submit.hpp"

#include "../vk/create.hpp"

namespace racecar::engine {

bool immediate_submit( vk::Common& vulkan,
                       ImmediateSubmit& immediate_submit,
                       std::function<void( VkCommandBuffer command_buffer )>&& function ) {
    RACECAR_VK_CHECK( vkResetFences( vulkan.device, 1, &immediate_submit.immediate_fence ),
                      "Failed to reset immediate fence" );
    RACECAR_VK_CHECK( vkResetCommandBuffer( immediate_submit.immediate_command_buffer, 0 ),
                      "Failed to reset immediate command buffer" );

    VkCommandBufferBeginInfo command_buffer_begin_info =
        vk::create::command_buffer_begin_info( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
    VkCommandBuffer command_buffer = immediate_submit.immediate_command_buffer;

    RACECAR_VK_CHECK( vkBeginCommandBuffer( command_buffer, &command_buffer_begin_info ),
                      "Failed to begin command buffer" );

    function( command_buffer );

    RACECAR_VK_CHECK( vkEndCommandBuffer( command_buffer ), "Failed to end command buffer" );

    VkCommandBufferSubmitInfo command_buffer_submit_info =
        vk::create::command_buffer_submit_info( command_buffer );
    VkSubmitInfo2 submit = vk::create::submit_info( &command_buffer_submit_info, nullptr, nullptr );

    RACECAR_VK_CHECK(
        vkQueueSubmit2( vulkan.graphics_queue, 1, &submit, immediate_submit.immediate_fence ),
        "Failed to submit immediate command to the queue" );
    RACECAR_VK_CHECK( vkWaitForFences( vulkan.device, 1, &immediate_submit.immediate_fence, true,
                                       std::numeric_limits<uint64_t>::max() ),
                      "Failed to wait for immediate fence following queue submit" );

    return true;
};

bool create_immediate_commands( ImmediateSubmit& immediate_submit, const vk::Common& vulkan ) {
    VkCommandPoolCreateInfo command_pool_info = vk::create::command_pool_info(
        vulkan.graphics_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT );

    RACECAR_VK_CHECK( vkCreateCommandPool( vulkan.device, &command_pool_info, nullptr,
                                           &immediate_submit.immediate_command_pool ),
                      "Failed to create immediate command pool" );

    VkCommandBufferAllocateInfo command_buffer_allocate_info =
        vk::create::command_buffer_allocate_info( immediate_submit.immediate_command_pool, 1 );

    RACECAR_VK_CHECK( vkAllocateCommandBuffers( vulkan.device, &command_buffer_allocate_info,
                                                &immediate_submit.immediate_command_buffer ),
                      "Failed to allocate immediate command buffer" );
    
    /// TODO: Call free when done

    return true;
};

bool create_immediate_sync_structures( ImmediateSubmit& immediate_submit, const vk::Common& vulkan ) {
    VkFenceCreateInfo fence_create_info = vk::create::fence_info( VK_FENCE_CREATE_SIGNALED_BIT );

    RACECAR_VK_CHECK( vkCreateFence( vulkan.device, &fence_create_info, nullptr,
                                     &immediate_submit.immediate_fence ),
                      "Failed to create immediate fence" );

    /// TODO: Get rid of this sync crap (call free)

    return true;
};

}  // namespace racecar::engine
