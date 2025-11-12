#pragma once

#include "draw_task.hpp"
#include "rwimage.hpp"

#include <volk.h>

#include <optional>
#include <vector>

namespace racecar::engine {

struct GfxTask {
    std::vector<DrawTask> draw_tasks;

    bool clear_screen = false;
    bool render_target_is_swapchain = false;

    std::vector<RWImage> color_attachments;
    RWImage depth_image;

    VkExtent2D extent = {};
};

std::optional<GfxTask> initialize_gfx_task();

void execute_gfx_task(
    const engine::State& engine, const VkCommandBuffer& cmd_buf, GfxTask& gfx_task );

} // namespace racecar::engine
