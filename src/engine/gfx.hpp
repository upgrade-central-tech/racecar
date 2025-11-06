#pragma once

#include "../context.hpp"
#include "pipeline.hpp"
#include "state.hpp"

#include <vector>

namespace racecar::engine {

/// Descriptor for a draw call.
struct DrawTaskDescriptor {
    /// User-defined parameters:
    Pipeline pipeline = {};
    VkShaderModule shader_module = {};
    VkExtent2D extent = {};
    bool clear_screen = false;

    /// Uninitialized by default below
    VkImage draw_target = VK_NULL_HANDLE;
    VkImageView draw_target_view = VK_NULL_HANDLE;
};

struct GfxTask {
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkSemaphore source_semaphore = VK_NULL_HANDLE;
    VkSemaphore target_semaphore = VK_NULL_HANDLE;

    std::optional<VkFence> fence;

    std::vector<DrawTaskDescriptor> draw_tasks;

    bool add_draw_task( DrawTaskDescriptor draw_task );
};

std::optional<GfxTask> create_gfx_task( const vk::Common& vulkan, const State& engine );
bool execute_gfx_task( const vk::Common& vulkan, const GfxTask& task );
void free_gfx_task( GfxTask& gfx_task );

bool draw( const DrawTaskDescriptor& draw_task, const VkCommandBuffer& cmd_buf );

}  // namespace racecar::engine
