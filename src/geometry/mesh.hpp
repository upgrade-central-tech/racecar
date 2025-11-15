#pragma once

#include "../engine/state.hpp"
#include "../vk/mem.hpp"

#include <glm/glm.hpp>

#include <array>
#include <span>

namespace racecar::geometry {

/// Vertex struct, not optimally arranged to fit 16 bytes.
struct Vertex {
    glm::vec4 color;
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 tangent;
    glm::vec2 uv;
};

struct GPUMeshBuffers {
    vk::mem::AllocatedBuffer index_buffer;
    vk::mem::AllocatedBuffer vertex_buffer;
    VkDeviceAddress vertex_buffer_address = 0;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    GPUMeshBuffers mesh_buffers;

    VkVertexInputBindingDescription vertex_binding_description = {
        .binding = vk::binding::VERTEX_BUFFER,
        .stride = sizeof( geometry::Vertex ),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    std::array<VkVertexInputAttributeDescription, 5> attribute_descriptions = { {
        { 0, vk::binding::VERTEX_BUFFER, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof( Vertex, color ) },
        { 1, vk::binding::VERTEX_BUFFER, VK_FORMAT_R32G32B32_SFLOAT, offsetof( Vertex, position ) },
        { 2, vk::binding::VERTEX_BUFFER, VK_FORMAT_R32G32B32_SFLOAT, offsetof( Vertex, normal ) },
        { 3, vk::binding::VERTEX_BUFFER, VK_FORMAT_R32G32B32_SFLOAT, offsetof( Vertex, tangent ) },
        { 4, vk::binding::VERTEX_BUFFER, VK_FORMAT_R32G32_SFLOAT, offsetof( Vertex, uv ) },
    } };
};

GPUMeshBuffers upload_mesh( vk::Common& vulkan, const engine::State& engine,
    std::span<uint32_t> indices, std::span<Vertex> vertices );

/// Should ideally run afer `scene::load_gltf` since it's just too annoying
/// to generate tangents while geo gets loaded and processed.
void generate_tangents( Mesh& mesh );

} // namespace racecar::geometry
