#pragma once

#include "blit_task.hpp"
#include "compute_task.hpp"
#include "gfx_task.hpp"
#include "pipeline_barrier.hpp"

#include <vector>

namespace racecar::engine {

using task_predicate_fn_t = bool ( * )( void );

/// To be space-efficient, a `Task` only stores the type (graphics, compute, blit)
/// and then an index into the corresponding list which is owned by `TaskList`.
struct Task {
    enum class Type { GFX, COMP, BLIT, CPU_CALL, GPU_CALL } type = Type::GFX;

    int index = -1;

    bool is_ran = false;
    bool is_single_run = false;

    task_predicate_fn_t predicate = nullptr;

    /// This allows you to skip writing "Type" e.g. you can just write `Task::GFX` or `Task::COMP`.
    using enum Type;
};

struct CPUTask {
    std::function<void()> task;
};

struct GPUTask {
    std::function<void( VkCommandBuffer )> task;
};

struct TaskList {
    std::vector<Task> tasks;

    std::vector<GfxTask> gfx_tasks;
    std::vector<ComputeTask> cs_tasks;
    std::vector<BlitTask> blit_tasks;
    std::vector<CPUTask> cpu_tasks;
    std::vector<GPUTask> gpu_tasks;

    std::vector<std::pair<int, PipelineBarrierDescriptor>> pipeline_barriers;
};

void add_gfx_task( TaskList& task_list, GfxTask task, task_predicate_fn_t predicate = nullptr );
void add_cs_task( TaskList& task_list, ComputeTask task, task_predicate_fn_t predicate = nullptr );
void add_blit_task( TaskList& task_list, BlitTask task, task_predicate_fn_t predicate = nullptr );
void add_pipeline_barrier( TaskList& task_list, PipelineBarrierDescriptor barrier );
void add_cpu_task( TaskList& task_list, std::function<void()> task );
void add_gpu_task( TaskList& task_list, std::function<void( VkCommandBuffer )> task );

} // namespace racecar::engine
