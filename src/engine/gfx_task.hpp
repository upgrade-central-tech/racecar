#pragma once

#include "../vk/common.hpp"
#include "draw_task.hpp"

#include <volk.h>

#include <vector>

namespace racecar::engine {

struct GfxTask {
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;

    /// This is the PREREQUISITE to our `GfxTask`. It is NOT owned by our `GfxTask`
    VkSemaphore wait_semaphore = VK_NULL_HANDLE;

    /// This is the semaphore we use to signal our COMPLETION. It IS owned by our `GfxTask`
    VkSemaphore signal_semaphore = VK_NULL_HANDLE;

    std::vector<DrawTaskDescriptor> draw_tasks;

    bool add_draw_task( DrawTaskDescriptor draw_task );
    void set_wait_semaphore( VkSemaphore wait_semaphore );
};

bool execute_gfx_task( const vk::Common& vulkan, const engine::State& engine, const GfxTask& task );
void free_gfx_task( const vk::Common& vulkan, const VkCommandPool cmd_pool, GfxTask& gfx_task );

bool draw( const vk::Common& vulkan, const engine::State& engine, const DrawTaskDescriptor& draw_task, const VkCommandBuffer& cmd_buf );

}  // namespace racecar::engine
