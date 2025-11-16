#include "draw_task.hpp"

#include "pipeline.hpp"

namespace racecar::engine {

void draw( const engine::State& engine, const DrawTask& draw_task, const VkCommandBuffer& cmd_buf,
    const VkExtent2D extent )
{
    vkCmdBindPipeline( cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, draw_task.pipeline.handle );

    // Dynamically set viewport state
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>( extent.width ),
        .height = static_cast<float>( extent.height ),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport( cmd_buf, 0, 1, &viewport );

    // Dynamically set scissor state
    VkRect2D scissor = {
        .offset = { .x = 0, .y = 0 },
        .extent = extent,
    };
    vkCmdSetScissor( cmd_buf, 0, 1, &scissor );

    for ( size_t i = 0; i < draw_task.descriptor_sets.size(); ++i ) {
        DescriptorSet* descriptor_set = draw_task.descriptor_sets[i];

        vkCmdBindDescriptorSets( cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
            draw_task.pipeline.layout, static_cast<uint32_t>( i ), 1,
            &descriptor_set->descriptor_sets[engine.get_frame_index()], 0, nullptr );
    }

    vkCmdBindVertexBuffers( cmd_buf, vk::binding::VERTEX_BUFFER,
        static_cast<uint32_t>( draw_task.draw_resource_descriptor.vertex_buffers.size() ),
        draw_task.draw_resource_descriptor.vertex_buffers.data(),
        draw_task.draw_resource_descriptor.vertex_buffer_offsets.data() );

    vkCmdBindIndexBuffer(
        cmd_buf, draw_task.draw_resource_descriptor.index_buffer, 0, VK_INDEX_TYPE_UINT32 );

    vkCmdDrawIndexed( cmd_buf, draw_task.draw_resource_descriptor.index_count, 1,
        uint32_t( draw_task.draw_resource_descriptor.first_index ),
        draw_task.draw_resource_descriptor.index_buffer_offset, 0 );
}

DrawResourceDescriptor DrawResourceDescriptor::from_mesh(
    const geometry::Mesh& mesh, const std::optional<scene::Primitive>& primitive )
{
    engine::DrawResourceDescriptor draw_mesh_desc = {
        .vertex_buffers = { mesh.mesh_buffers.vertex_buffer.handle },
        .index_buffer = mesh.mesh_buffers.index_buffer.handle,
        .vertex_buffer_offsets = { 0 },
        .index_buffer_offset = 0,
        .first_index = 0,
        .index_count = static_cast<uint32_t>( mesh.indices.size() ),
    };

    if ( primitive.has_value() ) {
        draw_mesh_desc.first_index = primitive->ind_offset;
        draw_mesh_desc.index_buffer_offset = primitive->vertex_offset;
        draw_mesh_desc.index_count = static_cast<uint32_t>( primitive->ind_count );
    }

    return draw_mesh_desc;
}

} // namespace racecar::engine
