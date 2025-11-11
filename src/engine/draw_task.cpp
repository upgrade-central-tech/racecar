#include "draw_task.hpp"

#include "pipeline.hpp"

namespace racecar::engine {

bool draw( [[maybe_unused]] vk::Common& vulkan,
           [[maybe_unused]] const engine::State& engine,
           const DrawTask& draw_task,
           const VkCommandBuffer& cmd_buf ) {  // Clear the background with a pulsing blue color
    VkClearColorValue clear_color = { { 0.0f, 0.0f, 1.0f, 1.0f } };

    VkRenderingAttachmentInfo color_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = draw_task.draw_target_view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = { .color = clear_color },
    };

    VkRenderingAttachmentInfo depth_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = draw_task.depth_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };

    // 1 is furthest away, this ultimately depends on your camera's near/far setup
    depth_attachment_info.clearValue.depthStencil.depth = 1.0f;

    VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { .offset = { .x = 0, .y = 0 }, .extent = draw_task.extent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_info,
        .pDepthAttachment = &depth_attachment_info,
        .pStencilAttachment = nullptr };

    vkCmdBeginRendering( cmd_buf, &rendering_info );

    {
        vkCmdBindPipeline( cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, draw_task.pipeline.handle );

        // Dynamically set viewport state
        VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>( draw_task.extent.width ),
            .height = static_cast<float>( draw_task.extent.height ),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport( cmd_buf, 0, 1, &viewport );

        // Dynamically set scissor state
        VkRect2D scissor = {
            .offset = { .x = 0, .y = 0 },
            .extent = draw_task.extent,
        };
        vkCmdSetScissor( cmd_buf, 0, 1, &scissor );

        for ( IUniformBuffer* buffer : draw_task.uniform_buffers ) {
            vkCmdBindDescriptorSets( cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                     draw_task.pipeline.layout, 0, 1,
                                     buffer->descriptor( engine.get_frame_index() ), 0, nullptr );
        }

        vkCmdBindVertexBuffers( cmd_buf, vk::binding::VERTEX_BUFFER,
                                draw_task.draw_resource_desc.vertex_buffers.size(),
                                draw_task.draw_resource_desc.vertex_buffers.data(),
                                draw_task.draw_resource_desc.vertex_buffer_offsets.data() );

        vkCmdBindIndexBuffer( cmd_buf, draw_task.draw_resource_desc.index_buffer, 0,
                              VK_INDEX_TYPE_UINT32 );

        vkCmdDrawIndexed( cmd_buf, draw_task.draw_resource_desc.index_count, 1,
                          draw_task.draw_resource_desc.first_index,
                          draw_task.draw_resource_desc.index_buffer_offset, 0 );
    }

    vkCmdEndRendering( cmd_buf );

    return true;
}

DrawResourceDescriptor DrawResourceDescriptor::from_mesh(
    const geometry::Mesh& mesh,
    const std::optional<scene::Primitive>& primitive ) {
    engine::DrawResourceDescriptor draw_mesh_desc{
        .vertex_buffers = { mesh.mesh_buffers.vertex_buffer.value().handle },
        .index_buffer = mesh.mesh_buffers.index_buffer.value().handle,
        .vertex_buffer_offsets = { 0 },
        .index_buffer_offset = 0,
        .first_index = 0,
        .index_count = static_cast<uint32_t>( mesh.indices.size() ) };

    if ( primitive.has_value() ) {
        draw_mesh_desc.first_index = primitive->ind_offset;
        draw_mesh_desc.index_buffer_offset = primitive->vertex_offset;
        draw_mesh_desc.index_count = primitive->ind_count;
    }

    return draw_mesh_desc;
}

}  // namespace racecar::engine
