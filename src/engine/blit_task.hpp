#pragma once

#include "image.h"


namespace racecar::engine {

struct BlitTask {
    /// Non-owning. Must outlive the task list. A null out_color blits to the swapchain image.
    engine::Image* in_color = nullptr;
    engine::Image* out_color = nullptr;
};

void execute_blit_task( const VkCommandBuffer& cmd_buf,
    BlitTask& blit_task, VkImage dst_image );

}
