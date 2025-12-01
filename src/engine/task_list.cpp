#include "task_list.hpp"

#include <SDL3/SDL.h>

namespace racecar::engine {

void add_gfx_task( TaskList& task_list, GfxTask task )
{
    Task new_task;
    new_task.index = static_cast<int>( task_list.tasks.size() );
    new_task.type = Task::GFX;

    task_list.tasks.push_back( new_task );
    task_list.gfx_tasks.push_back( task );
}

void add_cs_task( TaskList& task_list, ComputeTask task )
{
    Task new_task;
    new_task.index = static_cast<int>( task_list.tasks.size() );
    new_task.is_single_run = task.is_single_run;
    new_task.type = Task::Type::COMP;

    task_list.tasks.push_back( new_task );
    task_list.cs_tasks.push_back( task );
}

void add_blit_task( TaskList& task_list, BlitTask task )
{
    Task new_task;
    new_task.index = static_cast<int>( task_list.tasks.size() );
    new_task.type = Task::BLIT;

    task_list.tasks.push_back( new_task );
    task_list.blit_tasks.push_back( task );
}

void add_pipeline_barrier( TaskList& task_list, PipelineBarrierDescriptor barrier )
{
    task_list.pipeline_barriers.push_back(
        std::pair( static_cast<int>( task_list.tasks.size() ), barrier ) );
}

void add_cpu_task( TaskList& task_list, std::function<void()> task )
{
    Task new_task;
    new_task.index = static_cast<int>( task_list.tasks.size() );
    new_task.type = Task::CPU_CALL;

    task_list.tasks.push_back( new_task );
    task_list.cpu_tasks.push_back( { task } );
}

// TODO: This func and the above should be moved into its own file.
void transition_cs_read_to_write( engine::TaskList& task_list, engine::RWImage& image )
{
    engine::add_pipeline_barrier( task_list,
        engine::PipelineBarrierDescriptor { .buffer_barriers = {},
            .image_barriers = {
                engine::ImageBarrier {
                    .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .src_access = VK_ACCESS_2_SHADER_READ_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .dst_access = VK_ACCESS_2_SHADER_WRITE_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_GENERAL,
                    .image = image,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
                },
            } } );
}

void transition_cs_write_to_read( engine::TaskList& task_list, engine::RWImage& image )
{
    engine::add_pipeline_barrier( task_list,
        engine::PipelineBarrierDescriptor { .buffer_barriers = {},
            .image_barriers = {
                engine::ImageBarrier {
                    .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .src_access = VK_ACCESS_2_SHADER_WRITE_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_GENERAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .image = image,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
                },
            } } );
}

} // namespace racecar::engine
