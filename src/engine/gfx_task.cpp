#include "gfx_task.hpp"

namespace racecar::engine {

void execute_gfx_task( const vk::Common& vulkan,
                       const engine::State& engine,
                       const VkCommandBuffer& cmd_buf,
                       GfxTask& gfx_task ) {
    VkClearColorValue clear_color = { { 0.0f, 0.0f, 1.0f, 1.0f } };

    std::vector<VkRenderingAttachmentInfo> color_attachment_infos;

    size_t framedata_idx = gfx_task.render_target_is_swapchain ? 0 : engine.get_frame_index();

    for ( const RWImage& img : gfx_task.color_attachments ) {
        color_attachment_infos.push_back( {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = img.images[framedata_idx].image_view,
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp =
                gfx_task.clear_screen ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = { .color = clear_color },
        } );
    }

    VkRenderingAttachmentInfo depth_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = gfx_task.depth_image.images[framedata_idx].image_view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = gfx_task.clear_screen ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };

    // 1 is furthest away, this ultimately depends on your camera's near/far setup
    depth_attachment_info.clearValue.depthStencil.depth = 1.0f;

    VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { .offset = { .x = 0, .y = 0 }, .extent = gfx_task.extent },
        .layerCount = 1,
        .colorAttachmentCount = uint32_t( color_attachment_infos.size() ),
        .pColorAttachments = color_attachment_infos.data(),
        .pDepthAttachment = &depth_attachment_info,
        .pStencilAttachment = nullptr,
    };

    vkCmdBeginRendering( cmd_buf, &rendering_info );

    for ( DrawTask& draw_task : gfx_task.draw_tasks ) {
        // THIS NEEDS TO BE REMOVED AND REPLACED EVENTUALLY
        engine::update_images( vulkan, draw_task.images_descriptor, draw_task.textures,
                               int( engine.get_frame_index() ) );

        draw( engine, draw_task, cmd_buf, gfx_task.extent );
    }

    vkCmdEndRendering( cmd_buf );
}

}  // namespace racecar::engine
