#pragma once

#include "../geometry/mesh.hpp"
#include "pipeline.hpp"

#include <volk.h>

namespace racecar::engine {

/// Descriptor for a draw call.
struct DrawTask {
    /// User-defined parameters:
    std::optional<geometry::Mesh> mesh = {};
    Pipeline pipeline = {};
    VkShaderModule shader_module = {};
    VkExtent2D extent = {};
    bool clear_screen = false;

    bool render_target_is_swapchain = false;

    /// Uninitialized by default below
    VkImage draw_target = VK_NULL_HANDLE;
    VkImageView draw_target_view = VK_NULL_HANDLE;
};

bool draw( const DrawTask& draw_task, const VkCommandBuffer& cmd_buf );

}  // namespace racecar::engine
