#pragma once

#include "context.hpp"

namespace racecar::renderer {

struct Pipeline {
  VkPipeline handle = nullptr;
  VkPipelineLayout layout = nullptr;
};

/// Creates the graphics pipeline, if successful. Remember to call `free_pipeline` to destroy
/// the layout and other potential objects.
std::optional<Pipeline> create_gfx_pipeline(const vk::Common& vulkan);
void free_pipeline(const vk::Common& vulkan, Pipeline& pipeline);

bool draw(const Context& ctx, const Pipeline& gfx_pipeline);

}  // namespace racecar::renderer
