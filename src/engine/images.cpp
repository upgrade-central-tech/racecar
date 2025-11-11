#include "images.hpp"
#include "../vk/create.hpp"
#include "../vk/utility.hpp"

namespace racecar::engine {

ImagesDescriptor create_image_descriptors( vk::Common& vulkan,
                                           engine::State& engine,
                                           const std::vector<vk::mem::AllocatedImage>& images,
                                           VkShaderStageFlags shader_flags,
                                           uint32_t frame_overlap ) {
    ImagesDescriptor image_descriptor = {};

    engine::DescriptorLayoutBuilder builder;
    for ( uint32_t binding = 0; binding < images.size(); binding++ ) {
        engine::descriptor_layout_builder::add_binding( builder, binding,
                                                        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE );
    }

    image_descriptor.layout =
        engine::descriptor_layout_builder::build( vulkan, shader_flags, builder );

    image_descriptor.descriptor_sets = std::vector<VkDescriptorSet>(frame_overlap);
    
    for ( uint32_t i = 0; i < frame_overlap; i++ ) {
        image_descriptor.descriptor_sets[i] = engine::descriptor_allocator::allocate(
            vulkan, engine.descriptor_system.frame_allocators[i], image_descriptor.layout );
    }

    image_descriptor.images = std::vector<std::vector<vk::mem::AllocatedImage>>( frame_overlap );

    for ( uint32_t frame = 0; frame < frame_overlap; frame++ ) {
        image_descriptor.images[frame] = images;
    }

    return image_descriptor;
};

std::optional<vk::mem::AllocatedImage> create_image( vk::Common& vulkan,
                                                     engine::State& engine,
                                                     void* data,
                                                     VkExtent3D size,
                                                     VkFormat format,
                                                     VkImageUsageFlags usage,
                                                     bool mipmapped ) {
    std::optional<vk::mem::AllocatedImage> allocated_image =
        create_allocated_image( vulkan, engine, data, size, format, usage, mipmapped );

    if ( !allocated_image ) {
        SDL_Log( "Failed to allocate new image" );
        return {};
    }

    vulkan.destructor_stack.push( vulkan.device, allocated_image->image_view, vkDestroyImageView );
    vulkan.destructor_stack.push_free_vmaimage( vulkan.allocator, allocated_image.value() );

    return allocated_image;
};

std::optional<vk::mem::AllocatedImage> allocate_image( vk::Common& vulkan,
                                                       VkExtent3D size,
                                                       VkFormat format,
                                                       VkImageUsageFlags usage,
                                                       bool mipmapped ) {
    vk::mem::AllocatedImage new_image = {
        .image_extent = size,
        .image_format = format,
    };

    VkImageCreateInfo image_create_info = vk::create::image_create_info( format, usage, size );

    if ( mipmapped ) {
        image_create_info.mipLevels = static_cast<uint32_t>( std::floor(
                                          std::log2( std::max( size.width, size.height ) ) ) ) +
                                      1;
    }

    VmaAllocationCreateInfo allocation_create_info = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VkMemoryPropertyFlags( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ),
    };

    RACECAR_VK_CHECK( vmaCreateImage( vulkan.allocator, &image_create_info, &allocation_create_info,
                                      &new_image.image, &new_image.allocation, nullptr ),
                      "Failed to create VMA image" );

    VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
    if ( format == VK_FORMAT_D32_SFLOAT ) {
        aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageViewCreateInfo image_view_create_info =
        vk::create::image_view_create_info( format, new_image.image, aspect_flags );
    image_view_create_info.subresourceRange.levelCount = image_create_info.mipLevels;

    RACECAR_VK_CHECK(
        vkCreateImageView( vulkan.device, &image_view_create_info, nullptr, &new_image.image_view ),
        "Failed to create image view" );

    return new_image;
}

std::optional<vk::mem::AllocatedImage> create_allocated_image( vk::Common& vulkan,
                                                               engine::State& engine,
                                                               void* data,
                                                               VkExtent3D size,
                                                               VkFormat format,
                                                               VkImageUsageFlags usage,
                                                               bool mipmapped ) {
    size_t data_size =
        size.depth * size.width * size.height * vk::utility::bytes_from_format( format );
    std::optional<vk::mem::AllocatedBuffer> upload_buffer = vk::mem::create_buffer(
        vulkan, data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU );

    if ( !upload_buffer ) {
        SDL_Log( "[Vulkan] Failed to create upload buffer" );
        return {};
    }

    memcpy( upload_buffer.value().info.pMappedData, data, data_size );

    std::optional<vk::mem::AllocatedImage> new_image = allocate_image(
        vulkan, size, format,
        usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped );

    engine::immediate_submit(
        vulkan, engine.immediate_submit, [&]( VkCommandBuffer command_buffer ) {
            vk::utility::transition_image(
                command_buffer, new_image->image, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );

            VkBufferImageCopy copy_region = {
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
            };

            copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy_region.imageSubresource.mipLevel = 0;
            copy_region.imageSubresource.baseArrayLayer = 0;
            copy_region.imageSubresource.layerCount = 1;
            copy_region.imageExtent = size;

            vkCmdCopyBufferToImage( command_buffer, upload_buffer->handle, new_image->image,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region );
        } );

    vmaDestroyBuffer( vulkan.allocator, upload_buffer->handle, upload_buffer->allocation );

    return new_image;
}

void destroy_image( vk::Common& vulkan, const vk::mem::AllocatedImage& image ) {
    vkDestroyImageView( vulkan.device, image.image_view, nullptr );
    vmaDestroyImage( vulkan.allocator, image.image, image.allocation );
}

}  // namespace racecar::engine
