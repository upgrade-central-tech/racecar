#include "blit_task.hpp"

#include "../vk/utility.hpp"

namespace racecar::engine {

void execute_blit_task( const engine::State& engine, const VkCommandBuffer& cmd_buf,
    BlitTask& blit_task, VkImage dst_image )
{
    VkImage src_image = blit_task.screen_color.images[engine.get_frame_index()].image;

    VkImageBlit blit_region;
    blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit_region.srcSubresource.mipLevel = 0;
    blit_region.srcSubresource.baseArrayLayer = 0;
    blit_region.srcSubresource.layerCount = 1;
    blit_region.srcOffsets[0] = { 0, 0, 0 };
    blit_region.srcOffsets[1] = { static_cast<int32_t>( engine.swapchain.extent.width ),
        static_cast<int32_t>( engine.swapchain.extent.height ), 1 };

    blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit_region.dstSubresource.mipLevel = 0;
    blit_region.dstSubresource.baseArrayLayer = 0;
    blit_region.dstSubresource.layerCount = 1;
    blit_region.dstOffsets[0] = { 0, 0, 0 };
    blit_region.dstOffsets[1] = { static_cast<int32_t>( engine.swapchain.extent.width ),
        static_cast<int32_t>( engine.swapchain.extent.height ), 1 };

    vk::utility::transition_image( cmd_buf, dst_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_IMAGE_ASPECT_COLOR_BIT );

    vkCmdBlitImage( cmd_buf, src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit_region, VK_FILTER_NEAREST );

    vk::utility::transition_image( cmd_buf, dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_ASPECT_COLOR_BIT );
}

}
