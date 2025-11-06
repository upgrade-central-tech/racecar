#pragma once

#include "gfx_task.hpp"

#include <optional>
#include <vector>

namespace racecar::engine {

struct TaskList {
    std::vector<GfxTask> gfx_tasks;

    /// Attempts to add a `GfxTask` to the task list.
    TaskList& add_gfx_task( const vk::Common& vulkan, const State& engine );

    /// Call this last to finalize the creation of this task list.
    bool create( State& engine );
    std::optional<VkSemaphore> get_signal_semaphore();
};

void free_task_list(TaskList &task_list);

}  // namespace racecar::engine
