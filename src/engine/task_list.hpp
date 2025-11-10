#pragma once

#include "draw_task.hpp"

#include <vector>

namespace racecar::engine {

struct TaskList {
    std::vector<DrawTask> draw_tasks;
};

bool add_draw_task(TaskList &task_list, DrawTask task);

}  // namespace racecar::engine
