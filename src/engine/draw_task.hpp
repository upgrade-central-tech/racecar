#pragma once

#include "../geometry/mesh.hpp"
#include "../scene/scene.hpp"
#include "descriptor_set.hpp"
#include "pipeline.hpp"

#include <volk.h>

namespace racecar::engine {

struct DrawResourceDescriptor {
    std::vector<VkBuffer> vertex_buffers;
    VkBuffer index_buffer;

    std::vector<VkDeviceSize> vertex_buffer_offsets;
    int32_t index_buffer_offset;
    int32_t first_index;

    uint32_t index_count;

    static DrawResourceDescriptor from_mesh(
        const geometry::Mesh& mesh, const std::optional<scene::Primitive>& primitive );
};

/// A draw task represents one Vulkan pipeline, and more specifically, the shader to be used
/// in the pipeline. For example, you would use one draw task for each material, because each
/// material uses its own shader module.
struct DrawTask {
    DrawResourceDescriptor draw_resource_descriptor;
    std::vector<DescriptorSet*> descriptor_sets;
    Pipeline pipeline = {};
};

bool draw( const engine::State& engine, const DrawTask& draw_task, const VkCommandBuffer& cmd_buf,
    const VkExtent2D extent );

} // namespace racecar::engine
