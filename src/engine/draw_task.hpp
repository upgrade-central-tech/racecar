#pragma once

#include "../scene/scene.hpp"
#include "descriptor_set.hpp"
#include "pipeline.hpp"

#include <volk.h>

#include <optional>

namespace racecar::engine {

struct DrawResourceDescriptor {
    std::vector<VkBuffer> vertex_buffers;
    VkBuffer index_buffer = VK_NULL_HANDLE;

    std::vector<VkDeviceSize> vertex_buffer_offsets;
    int32_t vertex_offset = 0;
    int32_t index_offset = 0;

    uint32_t index_count = 0;

    static DrawResourceDescriptor from_mesh( VkBuffer vertex_buffer, VkBuffer index_buffer,
        uint32_t num_indices, const std::optional<scene::Primitive>& primitive );
};

/// A draw task represents one Vulkan pipeline, and more specifically, the shader to be used
/// in the pipeline. For example, you would use one draw task for each material, because each
/// material uses its own shader module.
struct DrawTask {
    DrawResourceDescriptor draw_resource_descriptor;
    std::vector<DescriptorSet*> descriptor_sets;
    Pipeline pipeline = {};
};

void draw( const engine::State& engine, const DrawTask& draw_task, const VkCommandBuffer& cmd_buf,
    const VkExtent2D extent );

} // namespace racecar::engine
