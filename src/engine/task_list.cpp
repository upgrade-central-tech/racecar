#include "task_list.hpp"

#include <SDL3/SDL.h>

namespace racecar::engine {

void add_gfx_task( TaskList& task_list, GfxTask task ) {
    task_list.gfx_tasks.push_back( task );
}

bool add_pipeline_barrier( TaskList& task_list, PipelineBarrierDescriptor barrier ) {
    task_list.pipeline_barriers.push_back( std::pair( task_list.gfx_tasks.size(), barrier ) );

    return true;
}

}  // namespace racecar::engine
