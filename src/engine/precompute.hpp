#pragma once

#include "../context.hpp"

#include <volk.h>

namespace racecar::engine {

VkFence create_fence();

void begin_precompute_commandbuffer(
    VkCommandBuffer* precompute_cmdbuf, const VkFence& precompute_fence );

void submit_precompute_cmdbuf(
    VkFence& precompute_fence, VkCommandBuffer& precompute_cmdbuf );

}
