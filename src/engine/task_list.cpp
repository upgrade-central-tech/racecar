#include "task_list.hpp"

#include <SDL3/SDL.h>

namespace racecar::engine {

void add_gfx_task( TaskList& task_list, GfxTask task, task_predicate_fn_t predicate )
{
    Task new_task;
    new_task.index = static_cast<int>( task_list.tasks.size() );
    new_task.type = Task::GFX;
    new_task.predicate = predicate;

    task_list.tasks.push_back( new_task );
    task_list.gfx_tasks.push_back( std::move( task ) );
}

void add_cs_task( TaskList& task_list, ComputeTask task, task_predicate_fn_t predicate )
{
    Task new_task;
    new_task.index = static_cast<int>( task_list.tasks.size() );
    new_task.is_single_run = task.is_single_run;
    new_task.type = Task::Type::COMP;
    new_task.predicate = predicate;

    task_list.tasks.push_back( new_task );
    task_list.cs_tasks.push_back( task );
}

void add_blit_task( TaskList& task_list, BlitTask task, task_predicate_fn_t predicate )
{
    Task new_task;
    new_task.index = static_cast<int>( task_list.tasks.size() );
    new_task.type = Task::BLIT;
    new_task.predicate = predicate;

    task_list.tasks.push_back( new_task );
    task_list.blit_tasks.push_back( task );
}

void add_pipeline_barrier( TaskList& task_list, PipelineBarrierDescriptor barrier )
{
    task_list.pipeline_barriers.push_back(
        std::pair( static_cast<int>( task_list.tasks.size() ), barrier )
    );
}

void add_cpu_task( TaskList& task_list, std::function<void()> task )
{
    Task new_task;
    new_task.index = static_cast<int>( task_list.tasks.size() );
    new_task.type = Task::CPU_CALL;

    task_list.tasks.push_back( new_task );
    task_list.cpu_tasks.push_back( { task } );
}

} // namespace racecar::engine
