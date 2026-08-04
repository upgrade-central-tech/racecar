#include "gfx_task.hpp"

namespace racecar::engine {

void execute_gfx_task( const VkCommandBuffer& cmd_buf, GfxTask& gfx_task )
{
    const engine::State& engine = engine::State::GetConst();
    const size_t frame_idx = engine.get_frame_index();

    std::vector<VkRenderingAttachmentInfo> color_attachment_infos;
    for ( const RWImage* img : gfx_task.color_attachments ) {
        color_attachment_infos.push_back(
            {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = img->images[frame_idx].image_view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp
                = gfx_task.clear_color ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = { .color = gfx_task.clear_color.value_or( VkClearColorValue { } ) },
            }
        );
    }

    std::optional<VkRenderingAttachmentInfo> depth_attachment_info;

    if ( gfx_task.depth_image != nullptr ) {
        depth_attachment_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = gfx_task.depth_image->images[frame_idx].image_view,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .loadOp
            = gfx_task.clear_depth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = { .depthStencil = { .depth = gfx_task.clear_depth.value_or( 0.f ) } },
        };
    }

    VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { .offset = { .x = 0, .y = 0 }, .extent = gfx_task.extent },
        .layerCount = 1,
        .colorAttachmentCount = static_cast<uint32_t>( color_attachment_infos.size() ),
        .pColorAttachments = color_attachment_infos.data(),
        .pDepthAttachment = depth_attachment_info ? &( *depth_attachment_info ) : nullptr,
        .pStencilAttachment = nullptr,
    };

    vkCmdBeginRendering( cmd_buf, &rendering_info );

    for ( const DrawTask& draw_task : gfx_task.draw_tasks ) {
        draw( draw_task, cmd_buf, gfx_task.extent );
    }

    vkCmdEndRendering( cmd_buf );
}

} // namespace racecar::engine
