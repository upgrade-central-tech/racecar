#pragma once

#include "../context.hpp"
#include "state.hpp"

#include <vulkan/vulkan_core.h>

#include <optional>

namespace racecar::engine::gui {

struct Gui {
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
};

std::optional<Gui> initialize( const Context& ctx, const State& engine );
void free( const vk::Common& vulkan, Gui& gui );

}  // namespace racecar::engine::gui
