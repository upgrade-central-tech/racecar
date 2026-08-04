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

    /// Non-owning. The images must outlive the task list, which is recorded once and replayed.
    std::vector<RWImage*> color_attachments;
    RWImage* depth_image = nullptr;

    VkExtent2D extent = {};
};

void execute_gfx_task(
    const VkCommandBuffer& cmd_buf, GfxTask& gfx_task );

} // namespace racecar::engine
