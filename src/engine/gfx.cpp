#include "gfx.hpp"

#include "pipeline.hpp"
#include "state.hpp"
#include "../vk/create.hpp"
#include "../vk/utility.hpp"

namespace racecar::engine {

bool GfxTask::add_draw_task( DrawTaskDescriptor draw_task ) {
    draw_tasks.push_back( draw_task );
    return true;
}

std::optional<GfxTask> create_gfx_task( const vk::Common& vulkan, const State& engine ) {
    GfxTask gfx_task = {};

    VkCommandPool cmd_pool = engine.gfx_command_pool;

    VkCommandBufferAllocateInfo cmd_buf_alloc_info =
        vk::create::command_buffer_allocate_info( cmd_pool, 1 );

    RACECAR_VK_CHECK(
        vkAllocateCommandBuffers( vulkan.device, &cmd_buf_alloc_info, &gfx_task.command_buffer ),
        "Failed to allocate command buffer" );

    // I don't really know what the initial flag should be, so I'm setting it to
    // VK_FENCE_CREATE_SIGNALED_BIT. The engine itself uses VK_FENCE_CREATE_SIGNALED_BIT because we
    // want to signal the fence is ready initially.
    VkFenceCreateInfo fence_create_info = vk::create::fence_info( VK_FENCE_CREATE_SIGNALED_BIT );

    RACECAR_VK_CHECK(
        vkCreateFence( vulkan.device, &fence_create_info, nullptr, &gfx_task.fence.value() ),
        "Failed to create render fence" );

    VkSemaphoreCreateInfo semaphore_create_info = vk::create::semaphore_info();

    RACECAR_VK_CHECK( vkCreateSemaphore( vulkan.device, &semaphore_create_info, nullptr,
                                         &gfx_task.source_semaphore ),
                      "Failed to create source semaphore" );

    RACECAR_VK_CHECK( vkCreateSemaphore( vulkan.device, &semaphore_create_info, nullptr,
                                         &gfx_task.target_semaphore ),
                      "Failed to create target semaphore" );

    return gfx_task;
}

bool execute_gfx_task( const vk::Common& vulkan, const GfxTask& task ) {
    VkCommandBuffer command_buffer = task.command_buffer;
    RACECAR_VK_CHECK( vkResetCommandBuffer( command_buffer, 0 ), "Failed to reset command buffer" );

    // Begin the command buffer, use one time flag for single usage (one submit
    // per frame).
    VkCommandBufferBeginInfo command_buffer_begin_info =
        vk::create::command_buffer_begin_info( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );

    RACECAR_VK_CHECK( vkBeginCommandBuffer( command_buffer, &command_buffer_begin_info ),
                      "Failed to begin command buffer" );

    for ( size_t i = 0; i < task.draw_tasks.size(); i++ ) {
        draw( task.draw_tasks[i], task.command_buffer );
    }

    RACECAR_VK_CHECK( vkEndCommandBuffer( command_buffer ), "Failed to end the command buffer" );

    // Prepare to submit our command to the graphics queue
    VkSemaphoreSubmitInfo wait_info = vk::create::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, task.target_semaphore );
    VkSemaphoreSubmitInfo signal_info = vk::create::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, task.source_semaphore );
    VkCommandBufferSubmitInfo command_info =
        vk::create::command_buffer_submit_info( task.command_buffer );
    VkSubmitInfo2 submit_info = vk::create::submit_info( &command_info, &signal_info, &wait_info );

    RACECAR_VK_CHECK( vkQueueSubmit2( vulkan.graphics_queue, 1, &submit_info,
                                      task.fence.value_or( VK_NULL_HANDLE ) ),
                      "Graphics queue submit failed" );

    return true;
}

bool draw( const DrawTaskDescriptor& draw_task, const VkCommandBuffer& cmd_buf ) {
    vk::utility::transition_image(
        cmd_buf, draw_task.draw_target, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT );

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

        vkCmdDraw( cmd_buf, 3, 1, 0, 0 );
    }

    vkCmdEndRendering( cmd_buf );

    // Transition image layout back so it can be presented on the screen
    vk::utility::transition_image(
        cmd_buf, draw_task.draw_target, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, 0,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT );

    return true;
}

}  // namespace racecar::engine
