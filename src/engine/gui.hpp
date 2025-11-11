#pragma once

#include "../context.hpp"
#include "state.hpp"

#include <vulkan/vulkan_core.h>

#include <optional>

namespace racecar::engine::gui {

struct Gui {
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
};

std::optional<Gui> initialize( Context& ctx, const State& engine );
void free();

}  // namespace racecar::engine::gui
