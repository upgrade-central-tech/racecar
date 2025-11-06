#include "mesh.hpp"

#include "imm_submit.hpp"

namespace racecar::engine {

std::optional<GPUMeshBuffers> upload_mesh( const vk::Common& vulkan,
                                           std::span<uint32_t> indices,
                                           std::span<Vertex> vertices ) {
    const size_t vertex_buffer_size = vertices.size() * sizeof( Vertex );
    const size_t index_buffer_size = indices.size() * sizeof( uint32_t );

    GPUMeshBuffers new_mesh_buffers;

    new_mesh_buffers.vertex_buffer = vk::mem::create_buffer(
        vulkan, vertex_buffer_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY );

    if ( !new_mesh_buffers.vertex_buffer ) {
        SDL_Log( "[Vulkan] Failed to create vertex buffer" );
        return {};
    }

    VkBufferDeviceAddressInfo device_address_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = new_mesh_buffers.vertex_buffer->handle,
    };

    new_mesh_buffers.vertex_buffer_address =
        vkGetBufferDeviceAddress( vulkan.device, &device_address_info );

    new_mesh_buffers.index_buffer =
        vk::mem::create_buffer( vulkan, index_buffer_size,
                                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                VMA_MEMORY_USAGE_GPU_ONLY );

    if ( !new_mesh_buffers.index_buffer ) {
        SDL_Log( "[Vulkan] Failed to create index buffer" );
        return {};
    }

    // Need to upload this to the buffer, analogous to DX12 default + upload heap
    std::optional<vk::mem::AllocatedBuffer> staging =
        vk::mem::create_buffer( vulkan, vertex_buffer_size + index_buffer_size,
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY );

    if ( !staging.has_value() ) {
        SDL_Log( "[Vulkan] Failed to create index buffer" );
        return {};
    }

    void* data = staging->info.pMappedData;

    memcpy( data, vertices.data(), vertex_buffer_size );
    memcpy( (char*)data + vertex_buffer_size, indices.data(), index_buffer_size );

    // Copy stitched buffer [ vertex_buffer_data, index_buffer_data ] to respective vertex and
    // index buffer data on GPU Use CmdCopyBuffer to transfer data from CPU to GPU
    immediate_submit( [&]( VkCommandBuffer command_buffer ) {
        VkBufferCopy vertexCopy = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = vertex_buffer_size,
        };

        vkCmdCopyBuffer( command_buffer, staging->handle, new_mesh_buffers.vertex_buffer->handle, 1,
                         &vertexCopy );

        VkBufferCopy indexCopy = {
            .srcOffset = vertex_buffer_size,
            .dstOffset = 0,
            .size = index_buffer_size,
        };

        vkCmdCopyBuffer( command_buffer, staging->handle, new_mesh_buffers.index_buffer->handle, 1,
                         &indexCopy );
    } );

    /// TODO: Destroy buffer immediately, maybe later verify if this needs to be in the deletion
    /// queue or not (as of writing, deletion queue not made)
    free_buffer( vulkan, staging.value() );

    return new_mesh_buffers;
}

}  // namespace racecar::engine
