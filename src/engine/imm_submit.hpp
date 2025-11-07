#pragma once

#include "../vk/common.hpp"

#include <functional>

namespace racecar::engine {

struct ImmediateSubmit {
    VkFence immediate_fence;
    VkCommandBuffer immediate_command_buffer;
    VkCommandPool immediate_command_pool;
};

/// Used for immediate command buffer calls, used for stuff like uploading buffer mem, etc.
bool immediate_submit( const vk::Common& vulkan,
                       const ImmediateSubmit& immediate_submit,
                       std::function<void( VkCommandBuffer command_buffer )>&& function );
bool create_immediate_commands( ImmediateSubmit& immediate_submit, const vk::Common& vulkan );
bool create_immediate_sync_structures( ImmediateSubmit& immediate_submit,
                                       const vk::Common& vulkan );

}  // namespace racecar::engine
