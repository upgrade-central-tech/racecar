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
    glm::vec2 uv;
};

struct GPUMeshBuffers {
    std::optional<vk::mem::AllocatedBuffer> index_buffer;
    std::optional<vk::mem::AllocatedBuffer> vertex_buffer;
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

    std::array<VkVertexInputAttributeDescription, 4> attribute_descriptions = { {
        { 0, vk::binding::VERTEX_BUFFER, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof( Vertex, color ) },
        { 1, vk::binding::VERTEX_BUFFER, VK_FORMAT_R32G32B32_SFLOAT, offsetof( Vertex, position ) },
        { 2, vk::binding::VERTEX_BUFFER, VK_FORMAT_R32G32B32_SFLOAT, offsetof( Vertex, normal ) },
        { 3, vk::binding::VERTEX_BUFFER, VK_FORMAT_R32G32_SFLOAT, offsetof( Vertex, uv ) },
    } };
};

std::optional<GPUMeshBuffers> upload_mesh( const vk::Common& vulkan,
                                           const engine::State& engine,
                                           std::span<uint32_t> indices,
                                           std::span<Vertex> vertices );
void free_mesh( const vk::Common& vulkan, Mesh& mesh );

}  // namespace racecar::geometry
