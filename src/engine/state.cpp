#include "state.hpp"

#include "../vk/create.hpp"

#include <algorithm>

namespace racecar::engine {

namespace {

std::optional<vkb::Swapchain> create_swapchain( SDL_Window* window, const vk::Common& vulkan ) {
    VkSurfaceCapabilitiesKHR capabilities = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( vulkan.device.physical_device, vulkan.surface,
                                               &capabilities );

    VkExtent2D swap_extent = { .width = 0, .height = 0 };
    if ( capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() ) {
        swap_extent = capabilities.currentExtent;
    } else {
        int width = 0;
        int height = 0;

        if ( !SDL_GetWindowSizeInPixels( window, &width, &height ) ) {
            SDL_Log( "[SDL] SDL_GetWindowSizeInPixels: %s", SDL_GetError() );
            return {};
        }

        swap_extent = {
            .width = std::clamp( static_cast<uint32_t>( width ), capabilities.minImageExtent.width,
                                 capabilities.maxImageExtent.width ),
            .height =
                std::clamp( static_cast<uint32_t>( height ), capabilities.minImageExtent.height,
                            capabilities.maxImageExtent.height ),
        };
    }

    vkb::SwapchainBuilder swapchain_builder( vulkan.device );
    vkb::Result<vkb::Swapchain> swapchain_ret =
        swapchain_builder.set_desired_extent( swap_extent.width, swap_extent.height )
            .set_desired_min_image_count( capabilities.minImageCount + 1 )
            .set_desired_present_mode( VK_PRESENT_MODE_FIFO_RELAXED_KHR )  // this is vsync
            .set_image_usage_flags( VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT )
            .build();

    if ( !swapchain_ret ) {
        auto swapchain_error = static_cast<vkb::SwapchainError>( swapchain_ret.error().value() );
        SDL_Log( "[vkb] Swapchain creation failed: %s", vkb::to_string( swapchain_error ) );
        return {};
    }

    const vkb::Swapchain& swapchain = swapchain_ret.value();

    SDL_Log( "[engine] Initial swapchain extent: %d×%d", swapchain.extent.width,
             swapchain.extent.height );

    return swapchain;
}

/// For each frame (of which there are as many as swapchain images), create a command buffer, and
/// synchronization primitives.
bool create_frame_data( State& engine, const vk::Common& vulkan ) {
    engine.frames = std::vector<FrameData>( engine.swapchain_images.size() );
    engine.frame_overlap = static_cast<uint32_t>( engine.swapchain_images.size() );
    engine.frame_number = 0;

    VkCommandPoolCreateInfo graphics_command_pool_info = vk::create::command_pool_info(
        vulkan.graphics_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT );

    // for ( uint32_t i = 0; i < engine.frame_overlap; i++ ) {
    // FrameData& frame = engine.frames[i];

    // RACECAR_VK_CHECK( vkCreateCommandPool( vulkan.device, &graphics_command_pool_info, nullptr,
    //    &frame.command_pool ),
    //   "Failed to create command pool" );

    // VkCommandBufferAllocateInfo cmd_buffer_allocate_info =
    // vk::create::command_buffer_allocate_info( frame.command_pool, 1 );

    // RACECAR_VK_CHECK( vkAllocateCommandBuffers( vulkan.device, &cmd_buffer_allocate_info,
    // &frame.command_buffer ),
    //   "Failed to allocate command buffer" );
    // }

    // Initialize the fence in a signaled state because otherwise, the first frame we wish to
    // draw will block indefinitely waiting for a signal to the fence that will never come
    // VkFenceCreateInfo fence_info = vk::create::fence_info( VK_FENCE_CREATE_SIGNALED_BIT );

    // for ( uint32_t i = 0; i < engine.frame_overlap; i++ ) {
    // FrameData* frame = &engine.frames[i];

    // RACECAR_VK_CHECK(
    // vkCreateFence( vulkan.device, &fence_info, nullptr, &frame->render_fence ),
    // "Failed to create render fence" );
    // }

    // Initialize the global command buffers

    RACECAR_VK_CHECK( vkCreateCommandPool( vulkan.device, &graphics_command_pool_info, nullptr,
                                           &engine.global_command_pool ),
                      "Failed to create global gfx command pool" );

    VkCommandBufferAllocateInfo global_start_cmd_buf_allocate_info =
        vk::create::command_buffer_allocate_info( engine.global_command_pool, 1 );

    VkCommandBufferAllocateInfo global_end_cmd_buf_allocate_info =
        vk::create::command_buffer_allocate_info( engine.global_command_pool, 1 );

    RACECAR_VK_CHECK( vkAllocateCommandBuffers( vulkan.device, &global_start_cmd_buf_allocate_info,
                                                &engine.global_start_cmd_buf ),
                      "Failed to create global gfx command buffer" );

    RACECAR_VK_CHECK( vkAllocateCommandBuffers( vulkan.device, &global_end_cmd_buf_allocate_info,
                                                &engine.global_end_cmd_buf ),
                      "Failed to create global gfx command buffer" );

    return true;
}

}  // namespace

std::optional<State> initialize( SDL_Window* window, const vk::Common& vulkan ) {
    State engine = {};

    if ( std::optional<vkb::Swapchain> swapchain_opt = create_swapchain( window, vulkan );
         !swapchain_opt ) {
        SDL_Log( "[Engine] Failed to create swapchain" );
        return {};
    } else {
        engine.swapchain = std::move( swapchain_opt.value() );
    }

    if ( vkb::Result<std::vector<VkImage>> images_res = engine.swapchain.get_images();
         !images_res ) {
        SDL_Log( "[vkb] Failed to get swapchain images: %s", images_res.error().message().c_str() );
        return {};
    } else {
        engine.swapchain_images = std::move( images_res.value() );
    }

    if ( vkb::Result<std::vector<VkImageView>> image_views_res = engine.swapchain.get_image_views();
         !image_views_res ) {
        SDL_Log( "[vkb] Failed to get swapchain image views: %s",
                 image_views_res.error().message().c_str() );
        return {};
    } else {
        engine.swapchain_image_views = std::move( image_views_res.value() );
    }

    if ( !create_frame_data( engine, vulkan ) ) {
        SDL_Log( "[Engine] Failed to create frame data" );
        return {};
    }

    if ( !create_immediate_commands( engine.immediate_submit, vulkan ) ) {
        SDL_Log( "[Engine] Failed to create immediate command buffer" );
        return {};
    }

    if ( !create_immediate_sync_structures( engine.immediate_submit, vulkan ) ) {
        SDL_Log( "[Engine] Failed to create immediate command sync fence" );
        return {};
    }

    if ( !create_descriptor_system( vulkan, engine.frame_overlap, engine.descriptor_system )) {
        SDL_Log("[Engine] Failed to create descriptor system");
        return {};
    }

    VkSemaphoreCreateInfo semaphore_info = vk::create::semaphore_info();

    RACECAR_VK_CHECK(
        vkCreateSemaphore( vulkan.device, &semaphore_info, nullptr, &engine.acquire_img_semaphore ),
        "Failed to create state semaphore" );

    RACECAR_VK_CHECK(
        vkCreateSemaphore( vulkan.device, &semaphore_info, nullptr, &engine.begin_gfx_semaphore ),
        "Failed to create state semaphore" );

    RACECAR_VK_CHECK( vkCreateSemaphore( vulkan.device, &semaphore_info, nullptr,
                                         &engine.present_image_signal_semaphore ),
                      "Failed to create state semaphore" );

    VkFenceCreateInfo fence_info = vk::create::fence_info( VK_FENCE_CREATE_SIGNALED_BIT );
    RACECAR_VK_CHECK( vkCreateFence( vulkan.device, &fence_info, nullptr, &engine.render_fence ),
                      "Failed to create render fence" );

    return engine;
}

void free( State& engine, [[maybe_unused]] const vk::Common& vulkan ) {
    // Kill per-frame resources from the engine
    // for ( uint32_t i = 0; i < engine.frame_overlap; i++ ) {
    //     // Kill command resources.
    //     vkDestroyCommandPool( vulkan.device, engine.frames[i].command_pool, nullptr );

    //     // Kill sync resources.
    //     vkDestroyFence( vulkan.device, engine.frames[i].render_fence, nullptr );
    // }

    engine.swapchain.destroy_image_views( engine.swapchain_image_views );
    vkb::destroy_swapchain( engine.swapchain );
}

}  // namespace racecar::engine
