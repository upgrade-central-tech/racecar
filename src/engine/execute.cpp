#include "execute.hpp"

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_vulkan.h"
#include "../vk/create.hpp"
#include "../vk/utility.hpp"
#include "task_list.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace racecar::engine {

void execute( State& engine, Context& ctx, TaskList& task_list )
{
    vk::Common& vulkan = ctx.vulkan;

    size_t frame_number = engine.get_frame_index();
    FrameData& frame = engine.frames[frame_number];

    // Using the maximum 64-bit unsigned integer value effectively disables the timeout
    vk::check( vkWaitForFences( vulkan.device, 1, &frame.render_fence, VK_TRUE,
                   std::numeric_limits<uint64_t>::max() ),
        "Failed to wait for frame render fence" );

    // Manually reset previous frame's render fence to an unsignaled state
    vk::check( vkResetFences( vulkan.device, 1, &frame.render_fence ),
        "Failed to reset frame render fence" );

    vkResetCommandBuffer( frame.start_cmdbuf, 0 );
    vkResetCommandBuffer( frame.render_cmdbuf, 0 );
    vkResetCommandBuffer( frame.end_cmdbuf, 0 );

    VkCommandBufferBeginInfo command_buffer_begin_info
        = vk::create::command_buffer_begin_info( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );

    // Request swapchain index.
    uint32_t output_swapchain_index = 0;

    vk::check( vkAcquireNextImageKHR( vulkan.device, engine.swapchain,
                   std::numeric_limits<uint64_t>::max(), frame.acquire_start_smp, nullptr,
                   &output_swapchain_index ),
        "Failed to acquire next image from swapchain" );

    const VkImage& output_image = engine.swapchain_images[output_swapchain_index];
    const VkImageView& output_image_view = engine.swapchain_image_views[output_swapchain_index];
    const vk::mem::AllocatedImage& out_depth_image = engine.depth_images[output_swapchain_index];

    SwapchainSemaphores& swapchain_semaphores = engine.swapchain_semaphores[output_swapchain_index];

    // For any render target rendering to the screen, set the dynamic output
    for ( GfxTask& gfx_task : task_list.gfx_tasks ) {
        if ( gfx_task.render_target_is_swapchain ) {
            gfx_task.color_attachments = {
                RWImage {
                    .images = { { .image = output_image, .image_view = output_image_view } } },
            };
            gfx_task.depth_image = { { out_depth_image } };
        }
    }

    {
        // Make swapchain image writeable ( and clear! )
        vkBeginCommandBuffer( frame.start_cmdbuf, &command_buffer_begin_info );
        vk::utility::transition_image( frame.start_cmdbuf, output_image, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT );

        // Pair in the depth here. Use start cmd_buf to ensure such is done before the draw call.
        vk::utility::transition_image( frame.start_cmdbuf, out_depth_image.image,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 0,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, VK_IMAGE_ASPECT_DEPTH_BIT );

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

    vk::check( vkQueueSubmit2( vulkan.graphics_queue, 1, &start_submit_info, VK_NULL_HANDLE ),
        "Graphics queue submit failed" );

    {
        vkBeginCommandBuffer( frame.render_cmdbuf, &command_buffer_begin_info );

        // THIS IS VERY BAD. THIS IS TEMPORARILY HERE SO I CAN RUN ANY ARBITRARY FUNCTION I WANT
        // WITH THE COMFORT OF KNOWING THE FRAME'S RENDER COMMAND BUFFER CAN BE USED. APOLOGIES.
        // LEAVING THIS HERE FOR NOW UNTIL WE HAVE A DECENT SOLUTION FOR COMPUTE TASKS THAT CAN RUN
        // DURING THE GAME. MAYBE YOU CAN THINK OF THIS AS A PRE-PASS BUFFER. I DON'T REALLY KNOW,
        // JUST FIGURE SOMETHING OUT BETTER.
        {
            for ( size_t i = 0; i < task_list.junk_tasks.size(); i++ ) {
                task_list.junk_tasks[i]( engine, ctx, frame );
            }
        }

        size_t task_ptr = 0;
        size_t gfx_ptr = 0;
        size_t cs_ptr = 0;
        size_t blit_ptr = 0;
        for ( task_ptr = 0; task_ptr < task_list.tasks.size(); task_ptr++ ) {
            Task& task = task_list.tasks[task_ptr];

#if 0
            // I'm leaving this chunk of code here in-case we want to loop back to its
            // ideas for refactoring later on. For now, the O(n) search work just fine as long
            // as we don't have a crazy amount of pipline barrieres per frame.
            auto search = std::find_if( task_list.pipeline_barriers.begin(),
                task_list.pipeline_barriers.end(),
                [=]( const std::pair<int, PipelineBarrierDescriptor>& v ) -> bool {
                    return v.first == int( task_ptr );
                } );

            if ( search != task_list.pipeline_barriers.end() ) {
                run_pipeline_barrier( engine, ( *search ).second, frame.render_cmdbuf );
            }
#else
            // Current implementation only handles 1 pipeline barrier for a given task_ptr.
            // Builder design assumes that you can append several, but that's not handled.

            // Naive for loop search.
            int task_index = static_cast<int>( task_ptr );

            for ( auto& [barrier_index, barrier] : task_list.pipeline_barriers ) {
                if ( barrier_index == task_index ) {
                    run_pipeline_barrier( engine, barrier, frame.render_cmdbuf );
                }
            }
#endif

            switch ( task.type ) {
            case Task::GFX: {
                GfxTask& gfx_task = task_list.gfx_tasks[gfx_ptr++];
                execute_gfx_task( engine, frame.render_cmdbuf, gfx_task );
                break;
            }

            case Task::COMP: {
                ComputeTask& cs_task = task_list.cs_tasks[cs_ptr++];
                execute_cs_task( engine, frame.render_cmdbuf, cs_task );
                break;
            }

            case Task::BLIT: {
                BlitTask& blit_task = task_list.blit_tasks[blit_ptr++];
                execute_blit_task( engine, frame.render_cmdbuf, blit_task, output_image );
                break;
            }

            default:
                throw Exception( "Unknown task type" );
            }
        }

        // GUI render pass
        {
            VkRenderingAttachmentInfo gui_color_attachment_info = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = output_image_view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            };

            VkRenderingInfo gui_rendering_info = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea = { .offset = { .x = 0, .y = 0 }, .extent = engine.swapchain.extent },
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &gui_color_attachment_info,
            };

            vkCmdBeginRendering( frame.render_cmdbuf, &gui_rendering_info );
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

    vk::check( vkQueueSubmit2( vulkan.graphics_queue, 1, &render_submit_info, VK_NULL_HANDLE ),
        "Graphics queue submit failed" );

    {
        vkBeginCommandBuffer( frame.end_cmdbuf, &command_buffer_begin_info );
        vk::utility::transition_image( frame.end_cmdbuf, output_image,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, 0,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT );
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

    vk::check( vkQueueSubmit2( vulkan.graphics_queue, 1, &end_submit_info, frame.render_fence ),
        "Graphics queue submit failed" );

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &swapchain_semaphores.end_present_smp,
        .swapchainCount = 1,
        .pSwapchains = &engine.swapchain.swapchain,
        .pImageIndices = &output_swapchain_index,
    };

    vk::check(
        vkQueuePresentKHR( vulkan.graphics_queue, &present_info ), "Failed to present to screen" );
}

} // namespace racecar::engine
