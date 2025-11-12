#pragma once

#include "draw_task.hpp"
#include "pipeline_barrier.hpp"

#include <vector>

namespace racecar::engine {

struct TaskList {
    std::vector<DrawTask> draw_tasks;
    std::vector<std::pair<int, PipelineBarrierDescriptor>> pipeline_barriers;
};

bool add_draw_task( TaskList& task_list, DrawTask task );
bool add_pipeline_barrier( PipelineBarrierDescriptor barrier );

}  // namespace racecar::engine
