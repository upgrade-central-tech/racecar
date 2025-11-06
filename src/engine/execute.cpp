#include "execute.hpp"

#include "../draw.hpp"
#include "../vk/create.hpp"
#include "../vk/utility.hpp"
#include "frame_data.hpp"

namespace racecar::engine {

bool execute( State& engine, const Context& ctx ) {
    const vk::Common& vulkan = ctx.vulkan;

    // This is the previous frame. We want to block the host until it has finished
    const FrameData* frame = &engine.frames[engine.frame_number];

    // Using the maximum 64-bit unsigned integer value effectively disables the timeout
    RACECAR_VK_CHECK( vkWaitForFences( vulkan.device, 1, &frame->render_fence, VK_TRUE,
                                       std::numeric_limits<uint64_t>::max() ),
                      "Failed to wait for frame render fence" );

    // Manually reset previous frame's render fence to an unsignaled state
    RACECAR_VK_CHECK( vkResetFences( vulkan.device, 1, &frame->render_fence ),
                      "Failed to reset frame render fence" );

    // Request swapchain index.
    uint32_t output_swapchain_index = 0;
    RACECAR_VK_CHECK( vkAcquireNextImageKHR(
                          vulkan.device, engine.swapchain, std::numeric_limits<uint64_t>::max(),
                          frame->swapchain_semaphore, nullptr, &output_swapchain_index ),
                      "Failed to acquire next image from swapchain" );

    const VkImage& output_image = engine.swapchain_images[output_swapchain_index];

    // Make swapchain image writeable
    vk::utility::transition_image(
        engine.global_gfx_cmd_buf, output_image, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT );

    // Rendering
    {
        std::optional<Pipeline> triangle_pipeline_opt =
            create_gfx_pipeline( engine, ctx.vulkan, "../shaders/triangle.spv" );

        if ( !triangle_pipeline_opt ) {
            SDL_Log( "[Engine] Failed to create pipeline" );
            return false;
        }

        std::optional<VkShaderModule> triangle_shader_module_opt =
            vk::create::shader_module( vulkan, "../shaders/triangle.spv" );

        if ( !triangle_shader_module_opt ) {
            SDL_Log( "[Engine] Failed to create shader module" );
            return {};
        }

        std::optional<GfxTask> render_triangle_task_opt = create_gfx_task( ctx, engine );

        if ( !render_triangle_task_opt ) {
            SDL_Log( "[Engine] Failed to create graphics task" );
            return false;
        }

        GfxTask& render_triangle_task = render_triangle_task_opt.value();
        VkShaderModule& triangle_shader_module = triangle_shader_module_opt.value();
        Pipeline& triangle_pipeline = triangle_pipeline_opt.value();

        render_triangle_task.add_draw_task( {
            .pipeline = triangle_pipeline,
            .shader_module = triangle_shader_module,
            .extent = engine.swapchain.extent,
            .clear_screen = true,
        } );

        execute_gfx_task( vulkan, render_triangle_task );
    }

    // Prepare to submit our command to the graphics queue
    VkSemaphoreSubmitInfo wait_info = vk::create::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, frame->swapchain_semaphore );
    VkSemaphoreSubmitInfo signal_info = vk::create::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, engine.render_semaphores[output_swapchain_index] );
    VkCommandBufferSubmitInfo command_info =
        vk::create::command_buffer_submit_info( engine.global_gfx_cmd_buf );
    VkSubmitInfo2 submit_info = vk::create::submit_info( &command_info, &signal_info, &wait_info );

    RACECAR_VK_CHECK( vkQueueSubmit2( vulkan.graphics_queue, 1, &submit_info, frame->render_fence ),
                      "Graphics queue submit failed" );

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &engine.render_semaphores[output_swapchain_index],
        .swapchainCount = 1,
        .pSwapchains = &engine.swapchain.swapchain,
        .pImageIndices = &output_swapchain_index,
    };

    RACECAR_VK_CHECK( vkQueuePresentKHR( vulkan.graphics_queue, &present_info ),
                      "Failed to present to screen" );

    return true;
}

}  // namespace racecar::engine
