#include "draw_task.hpp"

#include "pipeline.hpp"

namespace racecar::engine {

bool draw( vk::Common& vulkan,
           const engine::State& engine,
           const DrawTask& draw_task,
           const VkCommandBuffer& cmd_buf ) {  // Clear the background with a pulsing blue color
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

        for ( const LayoutResource& layout_resource : draw_task.layout_resources ) {
            // Copy from VkGuide
            vk::mem::AllocatedBuffer gpu_buffer =
                vk::mem::create_buffer( vulkan, layout_resource.data_size,
                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                        VMA_MEMORY_USAGE_CPU_TO_GPU )
                    .value();

            /// TODO: Setup cleanup

            // Map CPU to GPU mem
            memcpy( gpu_buffer.info.pMappedData, layout_resource.source_data,
                    layout_resource.data_size );

            // Create the descriptor set on the fly LOL
            // Ideally in double-buffering setup, we have the descriptor set available per frame.
            // Assume not, let's just hardcode 0 for the first
            // This should be ideally cached per frame. It's a waste of time to rebuild it every
            // time Or maybe, we could have a map or something that determines if such layout is
            // already pre-made. That, or we do a frames * layouts sized array containing every
            // descriptor set. That would be funny.
            VkDescriptorSet resource_descriptor = descriptor_allocator::allocate(
                vulkan, engine.descriptor_system.frame_allocators[0], layout_resource.layout );

            DescriptorWriter writer;
            write_buffer( writer, 0, gpu_buffer.handle, layout_resource.data_size, 0,
                          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
            update_set( writer, vulkan.device, resource_descriptor );

            vkCmdBindDescriptorSets( cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                     draw_task.pipeline.layout, 0, 1, &resource_descriptor, 0,
                                     nullptr );
        }

        // const geometry::Mesh& mesh = draw_task.mesh.value();
        // const geometry::GPUMeshBuffers& mesh_buffers = mesh.mesh_buffers;

        // VkBuffer vertex_buffer = mesh_buffers.vertex_buffer.value().handle;
        // VkBuffer index_buffer = mesh_buffers.index_buffer.value().handle;

        // VkDeviceSize offsets[] = { 0 };

        // uint32_t first_index = 0;
        // int32_t vertex_offset = 0;
        // uint32_t index_count = static_cast<uint32_t>( mesh.indices.size() );
        // if ( draw_task.primitive.has_value() ) {
        //     first_index = draw_task.primitive->ind_offset;
        //     vertex_offset = draw_task.primitive->vertex_offset;
        //     index_count = static_cast<uint32_t>(draw_task.primitive->ind_count);
        // }

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
