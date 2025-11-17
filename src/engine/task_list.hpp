#pragma once

#include "gfx_task.hpp"
#include "pipeline_barrier.hpp"

#include <vector>

namespace racecar::engine {

struct TaskList {
    std::vector<GfxTask> gfx_tasks;
    std::vector<std::pair<int, PipelineBarrierDescriptor>> pipeline_barriers;
};

void add_gfx_task( TaskList& task_list, GfxTask task );
void add_pipeline_barrier( TaskList& task_list, PipelineBarrierDescriptor barrier );

}  // namespace racecar::engine
