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
            .set_desired_min_image_count( capabilities.minImageCount )
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

    SDL_Log( "[Engine] Initial swapchain extent: %d×%d", swapchain.extent.width,
             swapchain.extent.height );

    return swapchain;
}

/// Generalized depth buffer creation per frame; for more robust depth textures, we may need a more
/// general purpose create_texture() function later on. Ideally, create_depth_images would therefore
/// re-use that create_texture() for each depth image.
bool create_depth_images( State& engine, vk::Common& vulkan ) {
    engine.depth_images = std::vector<vk::mem::AllocatedImage>( engine.frame_overlap );

    for ( uint32_t i = 0; i < engine.frame_overlap; i++ ) {
        vk::mem::AllocatedImage& depth_image = engine.depth_images[i];

        depth_image.image_format = VK_FORMAT_D32_SFLOAT;
        depth_image.image_extent = { engine.swapchain.extent.width, engine.swapchain.extent.height,
                                     1 };

        VkImageUsageFlags depth_image_usages = {};
        depth_image_usages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        VkImageCreateInfo depth_image_create_info = vk::create::image_create_info(
            depth_image.image_format, depth_image_usages, depth_image.image_extent );

        VmaAllocationCreateInfo image_allocate_info = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VkMemoryPropertyFlags( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) };

        RACECAR_VK_CHECK(
            vmaCreateImage( vulkan.allocator, &depth_image_create_info, &image_allocate_info,
                            &depth_image.image, &depth_image.allocation, nullptr ),
            "Failed to create depth image" );

        VkImageViewCreateInfo depth_view_create_info = vk::create::image_view_create_info(
            depth_image.image_format, depth_image.image, VK_IMAGE_ASPECT_DEPTH_BIT );

        RACECAR_VK_CHECK( vkCreateImageView( vulkan.device, &depth_view_create_info, nullptr,
                                             &depth_image.image_view ),
                          "Failed to create depth image view" );

        vulkan.destructor_stack.push( vulkan.device, depth_image.image_view, vkDestroyImageView );
        vulkan.destructor_stack.push_free_vmaimage( vulkan.allocator, depth_image );
    }

    return true;
}

/// For each frame (of which there are as many as swapchain images), create a command buffer, and
/// synchronization primitives.
bool create_frame_data( State& engine, vk::Common& vulkan ) {
    engine.frames = std::vector<FrameData>( engine.swapchain_images.size() );
    engine.swapchain_semaphores =
        std::vector<SwapchainSemaphores>( engine.swapchain_images.size() );
    engine.frame_overlap = static_cast<uint32_t>( engine.swapchain_images.size() );
    engine.frame_number = 0;

    const VkCommandBufferAllocateInfo cmdbuf_info =
        vk::create::command_buffer_allocate_info( engine.cmd_pool, 1 );
    const VkSemaphoreCreateInfo semaphore_info = vk::create::semaphore_info();
    const VkFenceCreateInfo fence_info = vk::create::fence_info( VK_FENCE_CREATE_SIGNALED_BIT );

    for ( uint32_t i = 0; i < engine.frame_overlap; i++ ) {
        FrameData& frame = engine.frames[i];
        SwapchainSemaphores& swapchain_semaphores = engine.swapchain_semaphores[i];

        RACECAR_VK_CHECK(
            vkAllocateCommandBuffers( vulkan.device, &cmdbuf_info, &frame.start_cmdbuf ),
            "Failed to create command buffer" );

        RACECAR_VK_CHECK(
            vkAllocateCommandBuffers( vulkan.device, &cmdbuf_info, &frame.render_cmdbuf ),
            "Failed to create command buffer" );

        RACECAR_VK_CHECK(
            vkAllocateCommandBuffers( vulkan.device, &cmdbuf_info, &frame.end_cmdbuf ),
            "Failed to create command buffer" );

        vulkan.destructor_stack.push_free_cmdbufs(
            vulkan.device, engine.cmd_pool,
            { frame.start_cmdbuf, frame.render_cmdbuf, frame.end_cmdbuf } );

        RACECAR_VK_CHECK(
            vkCreateSemaphore( vulkan.device, &semaphore_info, nullptr, &frame.start_render_smp ),
            "Failed to create state semaphore" );
        vulkan.destructor_stack.push( vulkan.device, frame.start_render_smp, vkDestroySemaphore );

        RACECAR_VK_CHECK(
            vkCreateSemaphore( vulkan.device, &semaphore_info, nullptr, &frame.render_end_smp ),
            "Failed to create state semaphore" );
        vulkan.destructor_stack.push( vulkan.device, frame.render_end_smp, vkDestroySemaphore );

        RACECAR_VK_CHECK(
            vkCreateSemaphore( vulkan.device, &semaphore_info, nullptr, &frame.acquire_start_smp ),
            "Failed to create state semaphore" );
        vulkan.destructor_stack.push( vulkan.device, frame.acquire_start_smp, vkDestroySemaphore );

        RACECAR_VK_CHECK( vkCreateSemaphore( vulkan.device, &semaphore_info, nullptr,
                                             &swapchain_semaphores.end_present_smp ),
                          "Failed to create state semaphore" );
        vulkan.destructor_stack.push( vulkan.device, swapchain_semaphores.end_present_smp,
                                      vkDestroySemaphore );

        RACECAR_VK_CHECK( vkCreateFence( vulkan.device, &fence_info, nullptr, &frame.render_fence ),
                          "Failed to create render fence" );
        vulkan.destructor_stack.push( vulkan.device, frame.render_fence, vkDestroyFence );
    }

    return true;
}

}  // namespace

std::optional<State> initialize( SDL_Window* window, vk::Common& vulkan ) {
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

    VkCommandPoolCreateInfo graphics_command_pool_info = vk::create::command_pool_info(
        vulkan.graphics_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT );
    RACECAR_VK_CHECK( vkCreateCommandPool( vulkan.device, &graphics_command_pool_info, nullptr,
                                           &engine.cmd_pool ),
                      "Failed to create command pool" );
    vulkan.destructor_stack.push( vulkan.device, engine.cmd_pool, vkDestroyCommandPool );

    if ( !create_frame_data( engine, vulkan ) ) {
        SDL_Log( "[Engine] Failed to create frame data" );
        return {};
    }

    if ( !create_depth_images( engine, vulkan ) ) {
        SDL_Log( "[Engine] Failed to create depth images/views" );
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

    if ( !create_descriptor_system( vulkan, engine.frame_overlap, engine.descriptor_system ) ) {
        SDL_Log( "[Engine] Failed to create descriptor system" );
        return {};
    }

    {
        scene::Camera& camera = engine.global_camera;

        camera.eye = glm::vec3( 0, 0, 3 );
        camera.look_at = glm::vec3( 0, 0, 0 );
        camera.up = glm::vec3( 0, 1, 0 );
        camera.forward = glm::normalize( camera.look_at - camera.eye );
        camera.right = glm::normalize( glm::cross( camera.forward, camera.up ) );
        camera.velocity = glm::vec3( 0 );

        camera.fov_y = glm::radians( 60.0 );
        camera.aspect_ratio = 1280.0 / 720.0;
        camera.near_plane = 0.1;
        camera.far_plane = 100.0;
    }
    return engine;
}

void free( State& engine, [[maybe_unused]] vk::Common& vulkan ) {
    vulkan.destructor_stack.execute_cleanup();

    engine.swapchain.destroy_image_views( engine.swapchain_image_views );
    vkb::destroy_swapchain( engine.swapchain );
}

}  // namespace racecar::engine
