#pragma once

#include "gfx_task.hpp"
#include "compute.hpp"
#include "pipeline_barrier.hpp"

#include <vector>

namespace racecar::engine {

struct BlitTask {
    engine::RWImage screen_color;
};

struct Task {
    std::optional<GfxTask> gfx_task;
    std::optional<ComputeTask> cs_task;
    std::optional<BlitTask> blit_task;
};

struct TaskList {
    // Vector are still needed, since the naive add tasks just get rid of 
    // struct member content. I don't know why.
    std::vector<GfxTask> gfx_tasks;
    std::vector<ComputeTask> cs_tasks;
    std::vector<BlitTask> blit_tasks;

    std::vector<Task> tasks;

    std::vector<std::pair<int, PipelineBarrierDescriptor>> pipeline_barriers;

    // Very dangerous. DON'T CHECK IN this code!
    std::vector<std::function<void(State&, Context&, FrameData&)>> junk_tasks;
};

void add_gfx_task( TaskList& task_list, GfxTask task );
void add_cs_task( TaskList& task_list, ComputeTask task );
void add_blit_task( TaskList& task_list, BlitTask task );
void add_pipeline_barrier( TaskList& task_list, PipelineBarrierDescriptor barrier );

} // namespace racecar::engine
