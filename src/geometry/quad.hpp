#pragma once

#include "../engine/state.hpp"
#include "../vk/common.hpp"
#include "gpu_mesh_buffers.hpp"

#include <glm/glm.hpp>
#include <volk.h>

#include <array>
#include <vector>

/// Fullscreen quad.
namespace racecar::geometry::quad {

/// For quads, vertex data is just vec3.
using Vertex = glm::vec2;

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    GPUMeshBuffers mesh_buffers;

    VkVertexInputBindingDescription vertex_binding_description = {
        .binding = vk::binding::VERTEX_BUFFER,
        .stride = sizeof( Vertex ),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    std::array<VkVertexInputAttributeDescription, 1> attribute_descriptions = { {
        { 0, vk::binding::VERTEX_BUFFER, VK_FORMAT_R32G32_SFLOAT, 0 },
    } };
};

Mesh create( vk::Common& vulkan, const engine::State& engine );

}
