#pragma once

#include "../context.hpp"

#include <volk.h>

namespace racecar::engine {

VkFence create_fence( const Context& ctx );

void begin_precompute_commandbuffer(
    const Context& ctx, VkCommandBuffer* precompute_cmdbuf, const VkFence& precompute_fence );

void submit_precompute_cmdbuf(
    vk::Common& vulkan, VkFence& precompute_fence, VkCommandBuffer& precompute_cmdbuf );

}
