#include "execute.hpp"

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_vulkan.h"
#include "../vk/create.hpp"
#include "../vk/utility.hpp"
#include "task_list.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace racecar::engine {

bool execute( State& engine, Context& ctx, TaskList& task_list )
{
    vk::Common& vulkan = ctx.vulkan;

    size_t frame_number = engine.get_frame_index();
    FrameData& frame = engine.frames[frame_number];

    // Using the maximum 64-bit unsigned integer value effectively disables the timeout
    RACECAR_VK_CHECK( vkWaitForFences( vulkan.device, 1, &frame.render_fence, VK_TRUE,
                          std::numeric_limits<uint64_t>::max() ),
        "Failed to wait for frame render fence" );

    // Manually reset previous frame's render fence to an unsignaled state
    RACECAR_VK_CHECK( vkResetFences( vulkan.device, 1, &frame.render_fence ),
        "Failed to reset frame render fence" );

    vkResetCommandBuffer( frame.start_cmdbuf, 0 );
    vkResetCommandBuffer( frame.render_cmdbuf, 0 );
    vkResetCommandBuffer( frame.end_cmdbuf, 0 );

    VkCommandBufferBeginInfo command_buffer_begin_info
        = vk::create::command_buffer_begin_info( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );

    // Request swapchain index.
    uint32_t output_swapchain_index = 0;
    RACECAR_VK_CHECK( vkAcquireNextImageKHR( vulkan.device, engine.swapchain,
                          std::numeric_limits<uint64_t>::max(), frame.acquire_start_smp, nullptr,
                          &output_swapchain_index ),
        "Failed to acquire next image from swapchain" );

    const VkImage& output_image = engine.swapchain_images[output_swapchain_index];
    const VkImageView& output_image_view = engine.swapchain_image_views[output_swapchain_index];

    const vk::mem::AllocatedImage& out_depth_image = engine.depth_images[output_swapchain_index];

    SwapchainSemaphores& swapchain_semaphores = engine.swapchain_semaphores[output_swapchain_index];

    // for any render target rendering to the screen, set the dynamic output
    for ( GfxTask& gfx_task : task_list.gfx_tasks ) {
        if ( gfx_task.render_target_is_swapchain ) {
            gfx_task.color_attachments
                = { { { { .image = output_image, .image_view = output_image_view } } } };
            gfx_task.depth_image = { { out_depth_image } };
        }
    }

    {
        // Make swapchain image writeable ( and clear! )
        vkBeginCommandBuffer( frame.start_cmdbuf, &command_buffer_begin_info );

        vk::utility::transition_image( frame.start_cmdbuf, output_image, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_TRANSFER_BIT );

        // Pair in the depth here. Use start cmd_buf to ensure such is done before the draw call.
        vk::utility::transition_image( frame.start_cmdbuf, out_depth_image.image,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
            VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT );

        VkClearColorValue clear_color = { { 1.0f, 1.0f, 1.0f, 1.0f } };
        VkImageSubresourceRange clear_range
            = vk::create::image_subresource_range( VK_IMAGE_ASPECT_COLOR_BIT );

        vkCmdClearColorImage( frame.start_cmdbuf, output_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &clear_range );

        VkClearDepthStencilValue clear_depth = { 1.0f, 0 };
        VkImageSubresourceRange clear_depth_range
            = vk::create::image_subresource_range( VK_IMAGE_ASPECT_DEPTH_BIT );

        vkCmdClearDepthStencilImage( frame.start_cmdbuf, out_depth_image.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_depth, 1, &clear_depth_range );

        vk::utility::transition_image( frame.start_cmdbuf, output_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT );

        // Pair in the depth here. Use start cmd_buf to ensure such is done before the draw call.
        vk::utility::transition_image( frame.start_cmdbuf, out_depth_image.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 0,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT );

        vkEndCommandBuffer( frame.start_cmdbuf );
    }

    vk::create::AllSubmitInfo start_submit_info_all = vk::create::all_submit_info( {
        .command_buffer = frame.start_cmdbuf,
        .wait_semaphore = frame.acquire_start_smp,
        .wait_flag_bits = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        .signal_semaphore = frame.start_render_smp,
        .signal_flag_bits = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
    } );
    VkSubmitInfo2 start_submit_info = vk::create::submit_info_from_all( start_submit_info_all );

    RACECAR_VK_CHECK(
        vkQueueSubmit2( vulkan.graphics_queue, 1, &start_submit_info, VK_NULL_HANDLE ),
        "Graphics queue submit failed" );

    {
        vkBeginCommandBuffer( frame.render_cmdbuf, &command_buffer_begin_info );

        for ( size_t i = 0; i < task_list.gfx_tasks.size(); i++ ) {
            auto search = std::find_if( task_list.pipeline_barriers.begin(),
                task_list.pipeline_barriers.end(),
                [=]( std::pair<int, PipelineBarrierDescriptor> v ) -> bool {
                    return v.first == int( i );
                } );

            if ( search != task_list.pipeline_barriers.end() ) {
                run_pipeline_barrier( ( *search ).second, frame.render_cmdbuf );
            }

            execute_gfx_task( engine, frame.render_cmdbuf, task_list.gfx_tasks[i] );
        }

        // GUI render pass
        // TODO: draw tasks currently need to specify many things (pipeline, shader module).
        // Therefore this step is currently hardcoded as part of the execution. Ideally we
        // incorporate it as part of the task system
        {
            VkRenderingAttachmentInfo color_attachment_info = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = output_image_view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            };

            VkRenderingInfo rendering_info = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea = { .offset = { .x = 0, .y = 0 }, .extent = engine.swapchain.extent },
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &color_attachment_info,
            };

            vkCmdBeginRendering( frame.render_cmdbuf, &rendering_info );
            ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), frame.render_cmdbuf );
            vkCmdEndRendering( frame.render_cmdbuf );
        }

        vkEndCommandBuffer( frame.render_cmdbuf );
    }

    vk::create::AllSubmitInfo render_submit_info_all = vk::create::all_submit_info( {
        .command_buffer = frame.render_cmdbuf,
        .wait_semaphore = frame.start_render_smp,
        .wait_flag_bits = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        .signal_semaphore = frame.render_end_smp,
        .signal_flag_bits = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
    } );
    VkSubmitInfo2 render_submit_info = vk::create::submit_info_from_all( render_submit_info_all );

    RACECAR_VK_CHECK(
        vkQueueSubmit2( vulkan.graphics_queue, 1, &render_submit_info, VK_NULL_HANDLE ),
        "Graphics queue submit failed" );

    {
        vkBeginCommandBuffer( frame.end_cmdbuf, &command_buffer_begin_info );
        vk::utility::transition_image( frame.end_cmdbuf, output_image,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, 0,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT );
        vkEndCommandBuffer( frame.end_cmdbuf );
    }

    vk::create::AllSubmitInfo end_submit_info_all = vk::create::all_submit_info( {
        .command_buffer = frame.end_cmdbuf,
        .wait_semaphore = frame.render_end_smp,
        .wait_flag_bits = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        .signal_semaphore = swapchain_semaphores.end_present_smp,
        .signal_flag_bits = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
    } );
    VkSubmitInfo2 end_submit_info = vk::create::submit_info_from_all( end_submit_info_all );

    RACECAR_VK_CHECK(
        vkQueueSubmit2( vulkan.graphics_queue, 1, &end_submit_info, frame.render_fence ),
        "Graphics queue submit failed" );

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &swapchain_semaphores.end_present_smp,
        .swapchainCount = 1,
        .pSwapchains = &engine.swapchain.swapchain,
        .pImageIndices = &output_swapchain_index,
    };

    RACECAR_VK_CHECK(
        vkQueuePresentKHR( vulkan.graphics_queue, &present_info ), "Failed to present to screen" );

    return true;
}

} // namespace racecar::engine
