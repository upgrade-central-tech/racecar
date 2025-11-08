#include "task_list.hpp"

#include "../vk/common.hpp"
#include "../vk/create.hpp"
#include "state.hpp"

#include <SDL3/SDL.h>

namespace racecar::engine {

TaskList& TaskList::add_gfx_task( const vk::Common& vulkan, const State& engine ) {
    GfxTask gfx_task = {};

    VkCommandBufferAllocateInfo cmd_buf_alloc_info =
        vk::create::command_buffer_allocate_info( engine.global_command_pool, 1 );

    if ( vkAllocateCommandBuffers( vulkan.device, &cmd_buf_alloc_info,
                                   &gfx_task.command_buffer ) ) {
        SDL_Log( "[TaskList::add_gfx_task] Failed to allocate command buffer" );
        return *this;
    }

    VkSemaphoreCreateInfo signal_semaphore_info = vk::create::semaphore_info();

    if ( vkCreateSemaphore( vulkan.device, &signal_semaphore_info, nullptr,
                            &gfx_task.signal_semaphore ) ) {
        SDL_Log( "[TaskList::add_gfx_task] Failed to create signal semaphore" );
        return *this;
    }

    if ( !gfx_tasks.empty() ) {
        const GfxTask& prev_gfx_task = gfx_tasks.back();
        gfx_task.set_wait_semaphore( prev_gfx_task.signal_semaphore );
    }

    gfx_tasks.push_back( std::move( gfx_task ) );

    return *this;
}

bool TaskList::create( State& engine ) {
    if ( gfx_tasks.empty() ) {
        SDL_Log( "[TASKLIST] Error: gfx_tasks is empty" );
        return false;
    }

    gfx_tasks[0].set_wait_semaphore( engine.begin_gfx_semaphore );
    return true;
}

std::optional<VkSemaphore> TaskList::get_signal_semaphore() {
    if ( gfx_tasks.empty() ) {
        SDL_Log( "[TASKLIST] Error: gfx_tasks is empty" );
        return {};
    }

    return gfx_tasks[gfx_tasks.size() - 1].signal_semaphore;
}

void free_task_list( vk::Common& vulkan, State& engine, TaskList& task_list ) {
    for ( auto& task : task_list.gfx_tasks ) {
        free_gfx_task( vulkan, engine.global_command_pool, task );
    }
}

}  // namespace racecar::engine
