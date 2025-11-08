#pragma once

#include <volk.h>

namespace racecar::engine {

/// Contains Vulkan objects for each image view in the swapchain.
struct FrameData {
    VkCommandPool command_pool = nullptr;
    VkCommandBuffer command_buffer = nullptr;
    VkFence render_fence = nullptr;
};

}  // namespace racecar::engine
