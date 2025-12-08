#pragma once

#include "../engine/state.hpp"
#include "../vk/mem.hpp"

#include <volk.h>

namespace racecar::geometry {

struct GPUMeshBuffers {
    vk::mem::AllocatedBuffer index_buffer;
    vk::mem::AllocatedBuffer vertex_buffer;
    VkDeviceAddress vertex_buffer_address = 0;

    size_t vertex_buffer_size = 0;
    size_t index_buffer_size = 0;
};

bool create_mesh_buffers( vk::Common& vulkan, GPUMeshBuffers& mesh_buffers,
    size_t vertex_buffer_size, size_t index_buffer_size );

bool upload_mesh_buffers( vk::Common& vulkan, engine::State& engine, GPUMeshBuffers& mesh_buffers,
    void* vertices_data, void* indices_data );

}
