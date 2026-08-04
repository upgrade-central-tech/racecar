#include "precompute.hpp"

#include "../vk/create.hpp"

namespace racecar::engine {

VkFence create_fence()
{
    const vk::Common& vulkan = vk::Common::GetConst();
    VkFenceCreateInfo fence_info = vk::create::fence_info( VK_FENCE_CREATE_SIGNALED_BIT );
    VkFence fence;
    vk::check(
        vkCreateFence( vulkan.device, &fence_info, nullptr, &fence ),
        "Failed to create precompute fence"
    );
    return fence;
}

void begin_precompute_commandbuffer(
    VkCommandBuffer* precompute_cmdbuf,
    const VkFence& precompute_fence
)
{
    const vk::Common& vulkan = vk::Common::GetConst();
    VkCommandBufferBeginInfo command_buffer_begin_info
        = vk::create::command_buffer_begin_info( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
    vkResetCommandBuffer( *precompute_cmdbuf, 0 );
    vkResetFences( vulkan.device, 1, &precompute_fence );
    vkBeginCommandBuffer( *precompute_cmdbuf, &command_buffer_begin_info );
}

void submit_precompute_cmdbuf(
    VkFence& precompute_fence, VkCommandBuffer& precompute_cmdbuf
)
{
    const vk::Common& vulkan = vk::Common::GetConst();
    vkEndCommandBuffer( precompute_cmdbuf );
    VkSubmitInfo submit_info = { };
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &precompute_cmdbuf;
    vkQueueSubmit( vulkan.graphics_queue, 1, &submit_info, precompute_fence );
    vkWaitForFences( vulkan.device, 1, &precompute_fence, VK_TRUE, UINT64_MAX );
    vkResetFences( vulkan.device, 1, &precompute_fence );
    vkDestroyFence( vulkan.device, precompute_fence, VK_NULL_HANDLE );
    vkResetCommandBuffer( precompute_cmdbuf, 0 );
}

}
