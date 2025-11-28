#pragma once

#include "rwimage.hpp"

namespace racecar::engine {

struct BlitTask {
    engine::RWImage screen_color;
};

void execute_blit_task( const engine::State& engine, const VkCommandBuffer& cmd_buf,
    BlitTask& blit_task, VkImage dst_image );

}
