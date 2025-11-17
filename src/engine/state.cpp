#include "state.hpp"

#include "../constants.hpp"
#include "../log.hpp"
#include "../vk/create.hpp"

#include <algorithm>

namespace racecar::engine {

namespace {

vkb::Swapchain create_swapchain( SDL_Window* window, const vk::Common& vulkan )
{
    VkSurfaceCapabilitiesKHR capabilities = {};

    vk::check( vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                   vulkan.device.physical_device, vulkan.surface, &capabilities ),
        "vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed" );

    VkExtent2D swap_extent = { .width = 0, .height = 0 };
    if ( capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() ) {
        swap_extent = capabilities.currentExtent;
    } else {
        int width = 0;
        int height = 0;

        if ( !SDL_GetWindowSizeInPixels( window, &width, &height ) ) {
            throw Exception( "[SDL] SDL_GetWindowSizeInPixels: {}", SDL_GetError() );
        }

        swap_extent = {
            .width = std::clamp( static_cast<uint32_t>( width ), capabilities.minImageExtent.width,
                capabilities.maxImageExtent.width ),
            .height = std::clamp( static_cast<uint32_t>( height ),
                capabilities.minImageExtent.height, capabilities.maxImageExtent.height ),
        };
    }

    vkb::SwapchainBuilder swapchain_builder( vulkan.device );
    vkb::Result<vkb::Swapchain> swapchain_ret
        = swapchain_builder.set_desired_extent( swap_extent.width, swap_extent.height )
              .set_desired_min_image_count( capabilities.minImageCount )
              .set_desired_present_mode( VK_PRESENT_MODE_FIFO_RELAXED_KHR ) // this is vsync
              .set_image_usage_flags( VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                  | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT )
              .build();

    if ( !swapchain_ret ) {
        auto swapchain_error = static_cast<vkb::SwapchainError>( swapchain_ret.error().value() );
        throw Exception( "[vkb] Swapchain creation failed: {}", vkb::to_string( swapchain_error ) );
    }

    const vkb::Swapchain& swapchain = swapchain_ret.value();

    log::info( "[engine] Initial swapchain extent: {}×{}", swapchain.extent.width,
        swapchain.extent.height );

    return swapchain;
}

void create_frame_data( State& engine, vk::Common& vulkan )
{
    size_t num_images = engine.swapchain_images.size();

    engine.frames = std::vector<FrameData>( num_images );
    engine.swapchain_semaphores = std::vector<SwapchainSemaphores>( num_images );
    engine.frame_overlap = static_cast<uint32_t>( num_images );
    engine.frame_number = 0;

    const VkCommandBufferAllocateInfo cmd_buf_info
        = vk::create::command_buffer_allocate_info( engine.cmd_pool, 1 );
    const VkSemaphoreCreateInfo semaphore_info = vk::create::semaphore_info();
    const VkFenceCreateInfo fence_info = vk::create::fence_info( VK_FENCE_CREATE_SIGNALED_BIT );

    for ( uint32_t i = 0; i < engine.frame_overlap; i++ ) {
        FrameData& frame = engine.frames[i];
        SwapchainSemaphores& swapchain_semaphores = engine.swapchain_semaphores[i];

        vk::check( vkAllocateCommandBuffers( vulkan.device, &cmd_buf_info, &frame.start_cmdbuf ),
            "Failed to create start command buffer" );
        vk::check( vkAllocateCommandBuffers( vulkan.device, &cmd_buf_info, &frame.render_cmdbuf ),
            "Failed to create render command buffer" );
        vk::check( vkAllocateCommandBuffers( vulkan.device, &cmd_buf_info, &frame.end_cmdbuf ),
            "Failed to create end command buffer" );

        vulkan.destructor_stack.push_free_cmd_bufs( vulkan.device, engine.cmd_pool,
            { frame.start_cmdbuf, frame.render_cmdbuf, frame.end_cmdbuf } );

        vk::check(
            vkCreateSemaphore( vulkan.device, &semaphore_info, nullptr, &frame.start_render_smp ),
            "Failed to create start render state semaphore" );
        vulkan.destructor_stack.push( vulkan.device, frame.start_render_smp, vkDestroySemaphore );

        vk::check(
            vkCreateSemaphore( vulkan.device, &semaphore_info, nullptr, &frame.render_end_smp ),
            "Failed to create end render state semaphore" );
        vulkan.destructor_stack.push( vulkan.device, frame.render_end_smp, vkDestroySemaphore );

        vk::check(
            vkCreateSemaphore( vulkan.device, &semaphore_info, nullptr, &frame.acquire_start_smp ),
            "Failed to create acquire start state semaphore" );
        vulkan.destructor_stack.push( vulkan.device, frame.acquire_start_smp, vkDestroySemaphore );

        vk::check( vkCreateSemaphore( vulkan.device, &semaphore_info, nullptr,
                       &swapchain_semaphores.end_present_smp ),
            "Failed to create end present state semaphore" );
        vulkan.destructor_stack.push(
            vulkan.device, swapchain_semaphores.end_present_smp, vkDestroySemaphore );

        vk::check( vkCreateFence( vulkan.device, &fence_info, nullptr, &frame.render_fence ),
            "Failed to create render fence" );
        vulkan.destructor_stack.push( vulkan.device, frame.render_fence, vkDestroyFence );
    }

    log::info( "[engine] Created all frame data" );
}

/// Generalized depth buffer creation per frame; for more robust depth textures, we may need a more
/// general purpose create_texture() function later on. Ideally, create_depth_images would therefore
/// re-use that create_texture() for each depth image.
void create_depth_images( State& engine, vk::Common& vulkan )
{
    engine.depth_images = std::vector<vk::mem::AllocatedImage>( engine.frame_overlap );

    for ( uint32_t i = 0; i < engine.frame_overlap; i++ ) {
        vk::mem::AllocatedImage& depth_image = engine.depth_images[i];

        depth_image.image_format = VK_FORMAT_D32_SFLOAT;
        depth_image.image_extent
            = { engine.swapchain.extent.width, engine.swapchain.extent.height, 1 };

        VkImageUsageFlags depth_image_usages = {};
        depth_image_usages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depth_image_usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        VkImageCreateInfo depth_image_info = vk::create::image_info( depth_image.image_format,
            VK_IMAGE_TYPE_2D, 1, 1, depth_image_usages, depth_image.image_extent );
        VmaAllocationCreateInfo image_allocate_info = { .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VkMemoryPropertyFlags( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) };

        vk::check( vmaCreateImage( vulkan.allocator, &depth_image_info, &image_allocate_info,
                       &depth_image.image, &depth_image.allocation, nullptr ),
            "[VMA] Failed to create depth image" );

        log::info( "[VMA] Allocated depth image {}", static_cast<void*>( depth_image.image ) );

        VkImageViewCreateInfo depth_view_create_info
            = vk::create::image_view_info( depth_image.image_format, depth_image.image,
                VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT );

        vk::check( vkCreateImageView(
                       vulkan.device, &depth_view_create_info, nullptr, &depth_image.image_view ),
            "Failed to create depth image view" );

        vulkan.destructor_stack.push( vulkan.device, depth_image.image_view, vkDestroyImageView );
        vulkan.destructor_stack.push_free_vmaimage( vulkan.allocator, depth_image );
    }

    log::info( "[engine] Created depth images and views" );
}

} // namespace

size_t State::get_frame_index() const
{
    return static_cast<size_t>( frame_number % frame_overlap );
}

State initialize( Context& ctx )
{
    vk::Common& vulkan = ctx.vulkan;
    State engine;

    try {
        engine.swapchain = create_swapchain( ctx.window, vulkan );

        {
            vkb::Result<std::vector<VkImage>> images_res = engine.swapchain.get_images();

            if ( !images_res ) {
                throw Exception(
                    "[vkb] Failed to get swapchain images: {}", images_res.error().message() );
            }

            engine.swapchain_images = std::move( images_res.value() );
        }

        {
            vkb::Result<std::vector<VkImageView>> image_views_res
                = engine.swapchain.get_image_views();

            if ( !image_views_res ) {
                throw Exception( "[vkb] Failed to get swapchain image views: {}",
                    image_views_res.error().message() );
            }

            engine.swapchain_image_views = std::move( image_views_res.value() );
        }

        {
            VkCommandPoolCreateInfo graphics_cmd_pool_info = vk::create::command_pool_info(
                vulkan.graphics_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT );

            vk::check( vkCreateCommandPool(
                           vulkan.device, &graphics_cmd_pool_info, nullptr, &engine.cmd_pool ),
                "Failed to create command pool" );

            vulkan.destructor_stack.push( vulkan.device, engine.cmd_pool, vkDestroyCommandPool );
        }

        create_frame_data( engine, vulkan );
        create_depth_images( engine, vulkan );

        engine.camera = {
            .center = {},
            .radius = 8.f,
            .azimuth = 0.f,
            .polar = 0.f,
            .up = glm::vec3( 0.f, 1.f, 0.f ),
            .fov_y = float( glm::radians( 60.0 ) ),
            .aspect_ratio = static_cast<float>( constant::SCREEN_W ) / constant::SCREEN_H,
            .near_plane = 0.1f,
            .far_plane = 100.f,
        };

        create_immediate_commands( engine.immediate_submit, vulkan );
        create_immediate_sync_structures( engine.immediate_submit, vulkan );
        create_descriptor_system( vulkan, engine.frame_overlap, engine.descriptor_system );
    } catch ( const Exception& ex ) {
        log::error( "[engine] {}", ex.what() );
        throw Exception( "[engine] Failed to initialize" );
    }

    log::info( "[engine] Initialized!" );

    return engine;
}

void free( State& engine )
{
    engine.swapchain.destroy_image_views( engine.swapchain_image_views );
    vkb::destroy_swapchain( engine.swapchain );
}

} // namespace racecar::engine
