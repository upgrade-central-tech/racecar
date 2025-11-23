#include "task_list.hpp"

#include <SDL3/SDL.h>

namespace racecar::engine {

void add_gfx_task( TaskList& task_list, GfxTask task ) { 
    Task new_task;
    new_task.gfx_task = task;
    task_list.tasks.push_back( new_task );
    
    task_list.gfx_tasks.push_back( task );
 }

void add_cs_task( TaskList& task_list, ComputeTask task ) { 
    Task new_task;
    new_task.cs_task = task;
    task_list.tasks.push_back( new_task );
    
    task_list.cs_tasks.push_back( task );
}

void add_blit_task( TaskList& task_list, BlitTask task ) {
    Task new_task;
    new_task.blit_task = task;
    task_list.tasks.push_back( new_task );
    
    task_list.blit_tasks.push_back( task );
}

void add_pipeline_barrier( TaskList& task_list, PipelineBarrierDescriptor barrier )
{
    task_list.pipeline_barriers.push_back(
        std::pair( static_cast<int>( task_list.tasks.size() ), barrier ) );
}

} // namespace racecar::engine
