#include "quad.hpp"

#include "../engine/imm_submit.hpp"
#include "../exception.hpp"
#include "../log.hpp"

namespace racecar::geometry::quad {

Mesh create( vk::Common& vulkan, const engine::State& engine )
{
    Mesh mesh = {
        .vertices = {
                glm::vec2( -1.f, -1.f ),
                glm::vec2( 1.f, -1.f ),
                glm::vec2( -1.f, 1.f ),
                glm::vec2( 1.f, 1.f ),
            },
            .indices = {2, 1, 0, 2, 3, 1},
    };

    const size_t vertex_buffer_size = mesh.vertices.size() * sizeof( Vertex );
    const size_t index_buffer_size = mesh.indices.size() * sizeof( uint32_t );

    try {
        mesh.mesh_buffers = {
            .index_buffer = vk::mem::create_buffer( vulkan, index_buffer_size,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VMA_MEMORY_USAGE_GPU_ONLY ),
            .vertex_buffer = vk::mem::create_buffer( vulkan, vertex_buffer_size,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                    | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VMA_MEMORY_USAGE_CPU_TO_GPU ),

        };
    } catch ( const Exception& ex ) {
        log::error( "[geometry::quad] Failed to create vertex + index buffers for quad" );
        throw;
    }

    {
        VkBufferDeviceAddressInfo device_address_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = mesh.mesh_buffers.vertex_buffer.handle,
        };

        mesh.mesh_buffers.vertex_buffer_address
            = vkGetBufferDeviceAddress( vulkan.device, &device_address_info );
    }

    // Upload index + vertex data to GPU
    {
        vk::mem::AllocatedBuffer staging
            = vk::mem::create_buffer( vulkan, vertex_buffer_size + index_buffer_size,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY );

        void* data = staging.info.pMappedData;
        std::memcpy( data, mesh.vertices.data(), vertex_buffer_size );
        std::memcpy( static_cast<char*>( data ) + vertex_buffer_size, mesh.indices.data(),
            index_buffer_size );

        // Copy stitched buffer [ vertex_buffer_data, index_buffer_data ] to respective vertex and
        // index buffer data on GPU Use CmdCopyBuffer to transfer data from CPU to GPU
        engine::immediate_submit( vulkan, engine.immediate_submit, [&]( VkCommandBuffer cmd_buf ) {
            VkBufferCopy vertex_buf_copy = {
                .srcOffset = 0,
                .dstOffset = 0,
                .size = vertex_buffer_size,
            };

            vkCmdCopyBuffer( cmd_buf, staging.handle, mesh.mesh_buffers.vertex_buffer.handle, 1,
                &vertex_buf_copy );

            VkBufferCopy index_buf_copy = {
                .srcOffset = vertex_buffer_size,
                .dstOffset = 0,
                .size = index_buffer_size,
            };

            vkCmdCopyBuffer( cmd_buf, staging.handle, mesh.mesh_buffers.index_buffer.handle, 1,
                &index_buf_copy );
        } );
    }

    return mesh;
}

}
