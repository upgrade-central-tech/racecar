#pragma once

#include "../vk/mem.hpp"

#include <glm/glm.hpp>

#include <span>

namespace racecar::engine {

struct Vertex {
    glm::vec4 color = {};
    glm::vec3 position = {};
    float uv_x = 0.0f;
    glm::vec3 normal = {};
    float uv_y = 0.0f;
};

struct GPUMeshBuffers {
    std::optional<vk::mem::AllocatedBuffer> index_buffer;
    std::optional<vk::mem::AllocatedBuffer> vertex_buffer;
    VkDeviceAddress vertex_buffer_address = 0;
};

std::optional<GPUMeshBuffers> upload_mesh( const vk::Common& vulkan,
                                          std::span<uint32_t> indices,
                                          std::span<Vertex> vertices );

}  // namespace racecar::engine
