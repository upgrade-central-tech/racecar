#pragma once

#include <volk.h>
#include "../vk/mem.hpp"

namespace racecar::engine {

/// Contains Vulkan objects for each image view in the swapchain.
/// **Any new buffers used for rendering must be defined here!**
struct FrameData {
    VkCommandPool command_pool = nullptr;
    VkCommandBuffer command_buffer = nullptr;
    VkFence render_fence = nullptr;

    vk::mem::UniformBuffer triangle_uniform_buffer;
};

}  // namespace racecar::engine
