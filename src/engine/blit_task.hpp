#pragma once

#include "rwimage.hpp"

#include <optional>

namespace racecar::engine {

struct BlitTask {
    engine::RWImage in_color;
    std::optional<engine::RWImage> out_color = {};
};

void execute_blit_task( const engine::State& engine, const VkCommandBuffer& cmd_buf,
    BlitTask& blit_task, VkImage dst_image );

}
