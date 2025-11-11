#pragma once

#include "../geometry/mesh.hpp"
#include "../scene/scene.hpp"
#include "pipeline.hpp"
#include "uniform_buffer.hpp"

#include <volk.h>

namespace racecar::engine {

struct DrawResourceDescriptor {
    std::vector<VkBuffer> vertex_buffers;
    VkBuffer index_buffer;

    std::vector<VkDeviceSize> vertex_buffer_offsets;
    int32_t index_buffer_offset;
    int32_t first_index;

    uint32_t index_count;

    static DrawResourceDescriptor from_mesh( const geometry::Mesh& mesh,
                                             const std::optional<scene::Primitive>& primitive );
};

/// Descriptor for a draw call.
struct DrawTask {
    /// User-defined parameters:
    DrawResourceDescriptor draw_resource_desc;
    std::vector<IUniformBuffer*> uniform_buffers;

    Pipeline pipeline = {};
    VkExtent2D extent = {};
    bool clear_screen = false;

    bool render_target_is_swapchain = false;

    /// Uninitialized by default below
    VkImage draw_target = VK_NULL_HANDLE;
    VkImageView draw_target_view = VK_NULL_HANDLE;

    VkImage depth_image = VK_NULL_HANDLE;
    VkImageView depth_image_view = VK_NULL_HANDLE;
};

bool draw( vk::Common& vulkan,
           const engine::State& engine,
           const DrawTask& draw_task,
           const VkCommandBuffer& cmd_buf );

}  // namespace racecar::engine
