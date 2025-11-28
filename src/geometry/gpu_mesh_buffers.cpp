#include "gpu_mesh_buffers.hpp"

#include "../log.hpp"

namespace racecar::geometry {

bool create_mesh_buffers( vk::Common& vulkan, GPUMeshBuffers& mesh_buffers,
    size_t vertex_buffer_size, size_t index_buffer_size )
{
    try {
        mesh_buffers = {
            .index_buffer = vk::mem::create_buffer( vulkan, index_buffer_size,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
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
            .buffer = mesh_buffers.vertex_buffer.handle,
        };

        mesh_buffers.vertex_buffer_address
            = vkGetBufferDeviceAddress( vulkan.device, &device_address_info );
    }

    mesh_buffers.vertex_buffer_size = vertex_buffer_size;
    mesh_buffers.index_buffer_size = index_buffer_size;

    return true;
}

bool upload_mesh_buffers( vk::Common& vulkan, engine::State& engine, GPUMeshBuffers& mesh_buffers,
    void* vertices_data, void* indices_data )
{
    // Upload index + vertex data to GPU
    {
        vk::mem::AllocatedBuffer staging = vk::mem::create_buffer( vulkan,
            mesh_buffers.vertex_buffer_size + mesh_buffers.index_buffer_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY );

        void* data = staging.info.pMappedData;
        std::memcpy( data, vertices_data, mesh_buffers.vertex_buffer_size );
        std::memcpy( static_cast<char*>( data ) + mesh_buffers.vertex_buffer_size, indices_data,
            mesh_buffers.index_buffer_size );

        // Copy stitched buffer [ vertex_buffer_data, index_buffer_data ] to respective vertex and
        // index buffer data on GPU Use CmdCopyBuffer to transfer data from CPU to GPU
        engine::immediate_submit( vulkan, engine.immediate_submit, [&]( VkCommandBuffer cmd_buf ) {
            VkBufferCopy vertex_buf_copy = {
                .srcOffset = 0,
                .dstOffset = 0,
                .size = mesh_buffers.vertex_buffer_size,
            };

            vkCmdCopyBuffer(
                cmd_buf, staging.handle, mesh_buffers.vertex_buffer.handle, 1, &vertex_buf_copy );

            VkBufferCopy index_buf_copy = {
                .srcOffset = mesh_buffers.vertex_buffer_size,
                .dstOffset = 0,
                .size = mesh_buffers.index_buffer_size,
            };

            vkCmdCopyBuffer(
                cmd_buf, staging.handle, mesh_buffers.index_buffer.handle, 1, &index_buf_copy );
        } );
    }

    return true;
}

}
