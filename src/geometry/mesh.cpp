#include "mesh.hpp"

#include "../engine/imm_submit.hpp"
#include "../log.hpp"

namespace racecar::geometry {

std::optional<GPUMeshBuffers> upload_mesh( vk::Common& vulkan, const engine::State& engine,
    std::span<uint32_t> indices, std::span<Vertex> vertices )
{
    const size_t vertex_buffer_size = vertices.size() * sizeof( Vertex );
    const size_t index_buffer_size = indices.size() * sizeof( uint32_t );

    GPUMeshBuffers new_mesh_buffers;

    new_mesh_buffers.vertex_buffer = vk::mem::create_buffer( vulkan, vertex_buffer_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU );

    if ( !new_mesh_buffers.vertex_buffer ) {
        log::error( "[Vulkan] Failed to create vertex buffer" );
        return {};
    }

    VkBufferDeviceAddressInfo device_address_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = new_mesh_buffers.vertex_buffer->handle,
    };

    new_mesh_buffers.vertex_buffer_address
        = vkGetBufferDeviceAddress( vulkan.device, &device_address_info );

    new_mesh_buffers.index_buffer = vk::mem::create_buffer( vulkan, index_buffer_size,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY );

    if ( !new_mesh_buffers.index_buffer ) {
        log::error( "[Vulkan] Failed to create index buffer" );
        return {};
    }

    // Need to upload this to the buffer, analogous to DX12 default + upload heap
    std::optional<vk::mem::AllocatedBuffer> staging
        = vk::mem::create_buffer( vulkan, vertex_buffer_size + index_buffer_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY );

    if ( !staging.has_value() ) {
        log::error( "[Vulkan] Failed to create index buffer" );
        return {};
    }

    void* data = staging->info.pMappedData;

    memcpy( data, vertices.data(), vertex_buffer_size );
    memcpy( (char*)data + vertex_buffer_size, indices.data(), index_buffer_size );

    // Copy stitched buffer [ vertex_buffer_data, index_buffer_data ] to respective vertex and
    // index buffer data on GPU Use CmdCopyBuffer to transfer data from CPU to GPU
    engine::immediate_submit(
        vulkan, engine.immediate_submit, [&]( VkCommandBuffer command_buffer ) {
            VkBufferCopy vertexCopy = {
                .srcOffset = 0,
                .dstOffset = 0,
                .size = vertex_buffer_size,
            };

            vkCmdCopyBuffer( command_buffer, staging->handle,
                new_mesh_buffers.vertex_buffer->handle, 1, &vertexCopy );

            VkBufferCopy indexCopy = {
                .srcOffset = vertex_buffer_size,
                .dstOffset = 0,
                .size = index_buffer_size,
            };

            vkCmdCopyBuffer( command_buffer, staging->handle, new_mesh_buffers.index_buffer->handle,
                1, &indexCopy );
        } );

    // Map GPU buffer temporarily for debug
    void* mapped = nullptr;
    vmaMapMemory( vulkan.allocator, new_mesh_buffers.vertex_buffer->allocation, &mapped );
    vmaUnmapMemory( vulkan.allocator, new_mesh_buffers.vertex_buffer->allocation );

    /// TODO: Destroy buffer immediately, maybe later verify if this needs to be in the deletion
    /// queue or not (as of writing, deletion queue not made)
    // vmaDestroyBuffer(vulkan.allocator, staging.value().handle, staging.value().allocation);

    return new_mesh_buffers;
}

void generate_tangents( Mesh& mesh )
{
    std::vector<geometry::Vertex>& vertices = mesh.vertices;
    std::vector<uint32_t>& indices = mesh.indices;

    // Shouldn't be necessary if default constructed to be 0, but do it in case
    for ( geometry::Vertex& vertex : vertices ) {
        vertex.tangent = glm::vec4( 0.0f );
    }

    for ( uint32_t index = 0; index < static_cast<uint32_t>( indices.size() ); index += 3 ) {
        uint32_t i0 = indices[index];
        uint32_t i1 = indices[index + 1];
        uint32_t i2 = indices[index + 2];

        const glm::vec3& pos0 = vertices[i0].position;
        const glm::vec3& pos1 = vertices[i1].position;
        const glm::vec3& pos2 = vertices[i2].position;

        const glm::vec2& uv0 = vertices[i0].uv;
        const glm::vec2& uv1 = vertices[i1].uv;
        const glm::vec2& uv2 = vertices[i2].uv;

        // https://learnopengl.com/Advanced-Lighting/Normal-Mapping
        glm::vec3 edge1 = pos1 - pos0;
        glm::vec3 edge2 = pos2 - pos0;

        glm::vec2 deltaUV1 = uv1 - uv0;
        glm::vec2 deltaUV2 = uv2 - uv0;

        float f = 1.0f / ( deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y );
        glm::vec3 tangent = f * ( deltaUV2.y * edge1 - deltaUV1.y * edge2 );
        glm::vec3 bitangent = f * ( -deltaUV2.x * edge1 + deltaUV1.x * edge2 );

        float w = glm::dot( normalize( glm::cross( vertices[i0].normal, tangent ) ),
                      normalize( bitangent ) )
                < 0.0f
            ? -1.0f
            : 0.0f;

        vertices[i0].tangent += glm::vec4( tangent, w );
        vertices[i1].tangent += glm::vec4( tangent, w );
        vertices[i2].tangent += glm::vec4( tangent, w );
    }

    for ( geometry::Vertex& vertex : vertices ) {
        glm::vec normal = glm::normalize( vertex.normal );
        glm::vec3 tangent
            = glm::normalize( glm::vec3( vertex.tangent.x, vertex.tangent.y, vertex.tangent.z ) );
        float tangent_w = vertex.tangent.w < 0.0f ? -1.0f : 1.0f;

        tangent = glm::normalize( tangent - normal * glm::dot( normal, tangent ) );

        vertex.tangent = glm::vec4( tangent, tangent_w );
    }
}

} // namespace racecar::geometry
