#pragma once

#include "blit_task.hpp"
#include "compute_task.hpp"
#include "gfx_task.hpp"
#include "pipeline_barrier.hpp"

#include <vector>


namespace racecar::engine {

/// To be space-efficient, a `Task` only stores the type (graphics, compute, blit)
/// and then an index into the corresponding list which is owned by `TaskList`.
struct Task {
    enum class Type { GFX, COMP, BLIT } type = Type::GFX;

    int index = -1;

    /// This allows you to skip writing "Type" e.g. you can just write `Task::GFX` or `Task::COMP`.
    using enum Type;
};

struct TaskList {
    std::vector<Task> tasks;

    std::vector<GfxTask> gfx_tasks;
    std::vector<ComputeTask> cs_tasks;
    std::vector<BlitTask> blit_tasks;

    std::vector<std::pair<int, PipelineBarrierDescriptor>> pipeline_barriers;

    // Very dangerous. DON'T CHECK IN this code!
    std::vector<std::function<void( State&, Context&, FrameData& )>> junk_tasks;
};

void add_gfx_task( TaskList& task_list, GfxTask task );
void add_cs_task( TaskList& task_list, ComputeTask task );
void add_blit_task( TaskList& task_list, BlitTask task );
void add_pipeline_barrier( TaskList& task_list, PipelineBarrierDescriptor barrier );

} // namespace racecar::engine
