#pragma once

#include "../vk/common.hpp"

#include <functional>

namespace racecar::engine {

struct ImmediateSubmit {
    VkFence fence = VK_NULL_HANDLE;
    VkCommandBuffer cmd_buf = VK_NULL_HANDLE;
    VkCommandPool cmd_pool = VK_NULL_HANDLE;
};

void create_immediate_commands( ImmediateSubmit& immediate_submit, vk::Common& vulkan );

/// Used for immediate command buffer calls, used for stuff like uploading buffer mem, etc.
void immediate_submit( const vk::Common& vulkan, const ImmediateSubmit& immediate_submit,
    std::function<void( VkCommandBuffer command_buffer )>&& function );

void create_immediate_sync_structures( ImmediateSubmit& immediate_submit, vk::Common& vulkan );

} // namespace racecar::engine
