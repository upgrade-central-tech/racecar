#pragma once

#include "context.hpp"

namespace racecar::renderer {

void create_graphics_pipeline(const vk::Common& vulkan);

std::optional<bool> draw(const Context& ctx);

}  // namespace racecar::renderer
