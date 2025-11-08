#include "execute.hpp"

#include "../vk/create.hpp"
#include "../vk/utility.hpp"
#include "task_list.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace racecar::engine {

bool execute( State& engine, const Context& ctx, TaskList& task_list ) {
    const vk::Common& vulkan = ctx.vulkan;

    {
        // Update the scene block. Hard-coded goodness.
        vk::mem::CameraBufferData& scene_camera_data = engine.descriptor_system.camera_data;

        glm::mat4 view = glm::lookAt( engine.global_camera.eye, engine.global_camera.look_at,
                                      engine.global_camera.up );

        /// TODO: Is there any reason why most of these camera struct values are doubles? Do we
        /// really need that much precision?
        glm::mat4 projection =
            glm::perspective( static_cast<float>( engine.global_camera.fov_y ),
                              static_cast<float>( engine.global_camera.aspect_ratio ),
                              static_cast<float>( engine.global_camera.near_plane ),
                              static_cast<float>( engine.global_camera.far_plane ) );

        projection[1][1] *= -1;

        // glm::mat4 model = glm::mat4( 1.0f );

        float angle = static_cast<float>( engine.rendered_frames ) * 0.1f; // in radians
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0, 1, 0)); // Y-axis rotation

        scene_camera_data.mvp = projection * view * model;
        scene_camera_data.color = glm::vec3(
            std::sin( static_cast<uint32_t>( engine.rendered_frames ) * 0.01f ), 0.0f, 0.0f );
    }

    // Using the maximum 64-bit unsigned integer value effectively disables the timeout
    RACECAR_VK_CHECK( vkWaitForFences( vulkan.device, 1, &engine.render_fence, VK_TRUE,
                                       std::numeric_limits<uint64_t>::max() ),
                      "Failed to wait for frame render fence" );

    RACECAR_VK_CHECK( vkResetDescriptorPool( vulkan.device,
                                             engine.descriptor_system.frame_allocators[0].pool, 0 ),
                      "Failed to reset frame descriptor pool" );

    // Manually reset previous frame's render fence to an unsignaled state
    RACECAR_VK_CHECK( vkResetFences( vulkan.device, 1, &engine.render_fence ),
                      "Failed to reset frame render fence" );

    vkResetCommandBuffer( engine.global_start_cmd_buf, 0 );
    vkResetCommandBuffer( engine.global_end_cmd_buf, 0 );

    VkCommandBufferBeginInfo command_buffer_begin_info =
        vk::create::command_buffer_begin_info( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );

    // Request swapchain index.
    uint32_t output_swapchain_index = 0;
    RACECAR_VK_CHECK( vkAcquireNextImageKHR(
                          vulkan.device, engine.swapchain, std::numeric_limits<uint64_t>::max(),
                          engine.acquire_img_semaphore, nullptr, &output_swapchain_index ),
                      "Failed to acquire next image from swapchain" );

    const VkImage& output_image = engine.swapchain_images[output_swapchain_index];
    const VkImageView& output_image_view = engine.swapchain_image_views[output_swapchain_index];

    for ( GfxTask& gfx_task : task_list.gfx_tasks ) {
        for ( DrawTaskDescriptor& draw_task : gfx_task.draw_tasks ) {
            if ( draw_task.render_target_is_swapchain ) {
                draw_task.draw_target = output_image;
                draw_task.draw_target_view = output_image_view;
            }
        }
    }

    {
        // Make swapchain image writeable
        vkBeginCommandBuffer( engine.global_start_cmd_buf, &command_buffer_begin_info );
        vk::utility::transition_image(
            engine.global_start_cmd_buf, output_image, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT );
        vkEndCommandBuffer( engine.global_start_cmd_buf );
    }

    vk::create::AllSubmitInfo global_start_submit_info_all = vk::create::all_submit_info( {
        .command_buffer = engine.global_start_cmd_buf,
        .wait_semaphore = engine.acquire_img_semaphore,
        .wait_flag_bits = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        .signal_semaphore = engine.begin_gfx_semaphore,
        .signal_flag_bits = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
    } );
    VkSubmitInfo2 global_start_submit_info =
        vk::create::submit_info_from_all( global_start_submit_info_all );

    RACECAR_VK_CHECK(
        vkQueueSubmit2( vulkan.graphics_queue, 1, &global_start_submit_info, VK_NULL_HANDLE ),
        "Graphics queue submit failed" );

    for ( auto& task : task_list.gfx_tasks ) {
        execute_gfx_task( vulkan, engine, task );
    }

    {
        vkBeginCommandBuffer( engine.global_end_cmd_buf, &command_buffer_begin_info );
        vk::utility::transition_image(
            engine.global_end_cmd_buf, output_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, 0,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT );
        vkEndCommandBuffer( engine.global_end_cmd_buf );
    }

    vk::create::AllSubmitInfo global_end_submit_info_all = vk::create::all_submit_info( {
        .command_buffer = engine.global_end_cmd_buf,
        .wait_semaphore = task_list.get_signal_semaphore().value(),
        .wait_flag_bits = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        .signal_semaphore = engine.present_image_signal_semaphore,
        .signal_flag_bits = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
    } );
    VkSubmitInfo2 global_end_submit_info =
        vk::create::submit_info_from_all( global_end_submit_info_all );

    RACECAR_VK_CHECK(
        vkQueueSubmit2( vulkan.graphics_queue, 1, &global_end_submit_info, engine.render_fence ),
        "Graphics queue submit failed" );

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &engine.present_image_signal_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &engine.swapchain.swapchain,
        .pImageIndices = &output_swapchain_index,
    };

    RACECAR_VK_CHECK( vkQueuePresentKHR( vulkan.graphics_queue, &present_info ),
                      "Failed to present to screen" );

    return true;
}

}  // namespace racecar::engine
