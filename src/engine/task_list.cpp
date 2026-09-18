#include "task_list.hpp"

#include <SDL3/SDL.h>

namespace racecar::engine {

namespace {

void set_task_name( [[maybe_unused]] Task& task, [[maybe_unused]] std::string_view name )
{
#if RACECAR_DEV
    task.name = name;
#endif
}

} // namespace

void add_gfx_task(
    TaskList& task_list, GfxTask task, std::string_view name, task_predicate_fn_t predicate
)
{
    Task new_task;
    new_task.index = static_cast<int>( task_list.tasks.size() );
    new_task.type = Task::GFX;
    new_task.predicate = predicate;
    set_task_name( new_task, name );

    task_list.tasks.push_back( new_task );
    task_list.gfx_tasks.push_back( std::move( task ) );
}

void add_cs_task(
    TaskList& task_list, ComputeTask task, std::string_view name, task_predicate_fn_t predicate
)
{
    Task new_task;
    new_task.index = static_cast<int>( task_list.tasks.size() );
    new_task.is_single_run = task.is_single_run;
    new_task.type = Task::Type::COMP;
    new_task.predicate = predicate;
    set_task_name( new_task, name );

    task_list.tasks.push_back( new_task );
    task_list.cs_tasks.push_back( task );
}

void add_blit_task(
    TaskList& task_list, BlitTask task, std::string_view name, task_predicate_fn_t predicate
)
{
    Task new_task;
    new_task.index = static_cast<int>( task_list.tasks.size() );
    new_task.type = Task::BLIT;
    new_task.predicate = predicate;
    set_task_name( new_task, name );

    task_list.tasks.push_back( new_task );
    task_list.blit_tasks.push_back( task );
}

void add_pipeline_barrier( TaskList& task_list, PipelineBarrierDescriptor barrier )
{
    task_list.pipeline_barriers.push_back(
        std::pair( static_cast<int>( task_list.tasks.size() ), barrier )
    );
}

void add_cpu_task( TaskList& task_list, std::function<void()> task, std::string_view name )
{
    Task new_task;
    new_task.index = static_cast<int>( task_list.tasks.size() );
    new_task.type = Task::CPU_CALL;
    set_task_name( new_task, name );

    task_list.tasks.push_back( new_task );
    task_list.cpu_tasks.push_back( { task } );
}

void add_gpu_task(
    TaskList& task_list, std::function<void( VkCommandBuffer )> task, std::string_view name
)
{
    Task new_task;
    new_task.index = static_cast<int>( task_list.tasks.size() );
    new_task.type = Task::GPU_CALL;
    set_task_name( new_task, name );

    task_list.tasks.push_back( new_task );
    task_list.gpu_tasks.push_back( { task } );
}

} // namespace racecar::engine
