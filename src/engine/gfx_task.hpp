#pragma once

#include "draw_task.hpp"
#include "rwimage.hpp"

#include <glm/glm.hpp>
#include <volk.h>

#include <optional>
#include <vector>

namespace racecar::engine {

struct GfxTask {
    std::vector<DrawTask> draw_tasks;

    std::optional<VkClearColorValue> clear_color;
    std::optional<float> clear_depth;

    bool render_target_is_swapchain = false;

    std::vector<RWImage> color_attachments;
    std::optional<RWImage> depth_image;

    VkExtent2D extent = {};
};

void execute_gfx_task(
    const engine::State& engine, const VkCommandBuffer& cmd_buf, GfxTask& gfx_task );

} // namespace racecar::engine
