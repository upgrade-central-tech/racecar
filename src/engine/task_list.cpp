#include "task_list.hpp"

#include <SDL3/SDL.h>

namespace racecar::engine {

bool add_draw_task( TaskList& task_list, DrawTask draw_task ) {
    task_list.draw_tasks.push_back( draw_task );
    return true;
}

bool add_pipeline_barrier( TaskList& task_list, PipelineBarrierDescriptor barrier ) {
    task_list.pipeline_barriers.push_back(std::pair(
        task_list.draw_tasks.size(),
        barrier
    ));

    return true;
}


}  // namespace racecar::engine
