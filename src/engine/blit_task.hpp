#pragma once

#include "rwimage.hpp"


namespace racecar::engine {

struct BlitTask {
    /// Non-owning. Must outlive the task list. A null out_color blits to the swapchain image.
    engine::RWImage* in_color = nullptr;
    engine::RWImage* out_color = nullptr;
};

void execute_blit_task( const engine::State& engine, const VkCommandBuffer& cmd_buf,
    BlitTask& blit_task, VkImage dst_image );

}
