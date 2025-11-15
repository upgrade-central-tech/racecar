#include "images.hpp"

#include "../log.hpp"
#include "../vk/create.hpp"
#include "../vk/utility.hpp"

namespace racecar::engine {

vk::mem::AllocatedImage create_image( vk::Common& vulkan, engine::State& engine, void* data,
    VkExtent3D extent, VkFormat format, VkImageType image_type, VkImageUsageFlags usage_flags,
    bool mipmapped )
{
    const size_t data_size
        = extent.depth * extent.width * extent.height * vk::utility::bytes_from_format( format );

    vk::mem::AllocatedImage new_image;

    try {
        vk::mem::AllocatedBuffer upload_buffer = vk::mem::create_buffer(
            vulkan, data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU );

        std::memcpy( upload_buffer.info.pMappedData, data, data_size );

        new_image = allocate_image( vulkan, extent, format, image_type,
            usage_flags | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            mipmapped );

        engine::immediate_submit(
            vulkan, engine.immediate_submit, [&]( VkCommandBuffer command_buffer ) {
                VkImageAspectFlags aspect_flags = format == VK_FORMAT_D32_SFLOAT
                    ? VK_IMAGE_ASPECT_DEPTH_BIT
                    : VK_IMAGE_ASPECT_COLOR_BIT;

                vk::utility::transition_image( command_buffer, new_image.image,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, aspect_flags );

                VkBufferImageCopy copy_region = {
                    .bufferOffset = 0,
                    .bufferRowLength = 0,
                    .bufferImageHeight = 0,
                };

                copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copy_region.imageSubresource.mipLevel = 0;
                copy_region.imageSubresource.baseArrayLayer = 0;
                copy_region.imageSubresource.layerCount = 1;
                copy_region.imageExtent = extent;

                vkCmdCopyBufferToImage( command_buffer, upload_buffer.handle, new_image.image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region );

                vk::utility::transition_image( command_buffer, new_image.image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    aspect_flags );
            } );
    } catch ( const Exception& ex ) {
        log::error( "[AllocatedImage] Error occurred: {}", ex.what() );
        throw;
    }

    return new_image;
};

vk::mem::AllocatedImage allocate_image( vk::Common& vulkan, VkExtent3D extent, VkFormat format,
    VkImageType image_type, VkImageUsageFlags usage_flags, bool mipmapped )
{
    vk::mem::AllocatedImage allocated_image = {
        .image_extent = extent,
        .image_format = format,
    };

    VkImageCreateInfo image_info
        = vk::create::image_info( format, image_type, usage_flags, extent );

    if ( mipmapped ) {
        image_info.mipLevels = static_cast<uint32_t>( std::floor(
                                   std::log2( std::max( extent.width, extent.height ) ) ) )
            + 1;
    }

    {
        VmaAllocationCreateInfo allocation_create_info = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VkMemoryPropertyFlags( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ),
        };
        vk::check( vmaCreateImage( vulkan.allocator, &image_info, &allocation_create_info,
                       &allocated_image.image, &allocated_image.allocation, nullptr ),
            "[VMA] Failed to create image" );
        vulkan.destructor_stack.push_free_vmaimage( vulkan.allocator, allocated_image );

        log::info( "Allocated image {}", static_cast<void*>( allocated_image.image ) );
    }

    {
        VkImageViewCreateInfo image_view_info
            = vk::create::image_view_info( format, allocated_image.image,
                format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT
                                               : VK_IMAGE_ASPECT_COLOR_BIT );
        image_view_info.subresourceRange.levelCount = image_info.mipLevels;
        vk::check( vkCreateImageView(
                       vulkan.device, &image_view_info, nullptr, &allocated_image.image_view ),
            "Failed to create image view" );
        vulkan.destructor_stack.push(
            vulkan.device, allocated_image.image_view, vkDestroyImageView );
    }

    return allocated_image;
}

} // namespace racecar::engine
