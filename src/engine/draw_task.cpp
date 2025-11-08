#include "draw_task.hpp"

#include "pipeline.hpp"


namespace racecar::engine {

bool draw( const DrawTask& draw_task, const VkCommandBuffer& cmd_buf ) {
    // Clear the background with a pulsing blue color
    // float flash = std::abs( std::sin( static_cast<float>( vulkan.rendered_frames ) / 120.f ) );
    VkClearColorValue clear_color = { { 0.0f, 0.0f, 1.0f, 1.0f } };

    VkRenderingAttachmentInfo color_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = draw_task.draw_target_view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = { .color = clear_color },
    };

    VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { .offset = { .x = 0, .y = 0 }, .extent = draw_task.extent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_info,
    };

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

        if ( draw_task.mesh.has_value() ) {
            const geometry::Mesh& mesh = draw_task.mesh.value();
            const geometry::GPUMeshBuffers& mesh_buffers = mesh.mesh_buffers;

            VkBuffer vertex_buffer = mesh_buffers.vertex_buffer.value().handle;
            VkBuffer index_buffer = mesh_buffers.index_buffer.value().handle;

            VkDeviceSize offsets[] = { 0 };

            vkCmdBindVertexBuffers( cmd_buf, vk::binding::VERTEX_BUFFER, 1, &vertex_buffer,
                                    offsets );
            vkCmdBindIndexBuffer( cmd_buf, index_buffer, 0, VK_INDEX_TYPE_UINT32 );

            vkCmdDrawIndexed( cmd_buf, static_cast<uint32_t>( mesh.indices.size() ), 1, 0, 0, 0 );
        } else {
            // HARDCODED draw! This shouldn't be needed if we force users to draw via vertex buffer.
            vkCmdDraw( cmd_buf, 3, 1, 0, 0 );
        }
    }

    vkCmdEndRendering( cmd_buf );

    return true;
}

}  // namespace racecar::engine
