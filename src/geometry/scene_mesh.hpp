#pragma once

#include "../engine/state.hpp"
#include "gpu_mesh_buffers.hpp"

#include <glm/glm.hpp>
#include <volk.h>

#include <array>
#include <span>

namespace racecar::geometry::scene {

/// Vertex struct, not optimally arranged to fit 16 bytes.
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 tangent;
    glm::vec2 uv;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    GPUMeshBuffers mesh_buffers;

    VkVertexInputBindingDescription vertex_binding_description = {
        .binding = vk::binding::VERTEX_BUFFER,
        .stride = sizeof( Vertex ),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    std::array<VkVertexInputAttributeDescription, 4> attribute_descriptions = { {
        { 0, vk::binding::VERTEX_BUFFER, VK_FORMAT_R32G32B32_SFLOAT, offsetof( Vertex, position ) },
        { 1, vk::binding::VERTEX_BUFFER, VK_FORMAT_R32G32B32_SFLOAT, offsetof( Vertex, normal ) },
        { 2, vk::binding::VERTEX_BUFFER, VK_FORMAT_R32G32B32_SFLOAT, offsetof( Vertex, tangent ) },
        { 3, vk::binding::VERTEX_BUFFER, VK_FORMAT_R32G32_SFLOAT, offsetof( Vertex, uv ) },
    } };
};

GPUMeshBuffers upload_mesh( vk::Common& vulkan, const engine::State& engine,
    std::span<uint32_t> indices, std::span<Vertex> vertices );

/// Should ideally run afer `scene::load_gltf` since it's just too annoying
/// to generate tangents while geo gets loaded and processed.
void generate_tangents( Mesh& mesh );

} // namespace racecar::geometry
