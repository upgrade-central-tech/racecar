#include "images.hpp"

#include "../log.hpp"
#include "../vk/create.hpp"
#include "../vk/utility.hpp"

#include <stb_image.h>

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

        new_image = allocate_image( vulkan, extent, format, image_type, 1, 1, VK_SAMPLE_COUNT_1_BIT,
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

vk::mem::AllocatedImage allocate_vma_image( vk::Common& vulkan, VkExtent3D extent, VkFormat format,
    VkImageType image_type, uint32_t mip_levels, uint32_t array_layers,
    VkSampleCountFlagBits samples, VkImageUsageFlags usage_flags )
{
    vk::mem::AllocatedImage allocated_image = {
        .image_extent = extent,
        .image_format = format,
    };

    VkImageCreateInfo image_info = vk::create::image_info(
        format, image_type, mip_levels, array_layers, samples, usage_flags, extent );

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
        // Let's just assume there are binary results
        VkImageViewType image_view_type
            = ( image_type == VK_IMAGE_TYPE_2D ) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_3D;

        VkImageViewCreateInfo image_view_info
            = vk::create::image_view_info( format, allocated_image.image, image_view_type,
                format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT
                                               : VK_IMAGE_ASPECT_COLOR_BIT );
        image_view_info.subresourceRange.levelCount = image_info.mipLevels;
        vk::check( vkCreateImageView(
                       vulkan.device, &image_view_info, nullptr, &allocated_image.image_view ),
            "Failed to create image view" );

        // They can be the same, as long as VK_IMAGE_USAGE_STORAGE_BIT is used. The views are
        // different, however, for writing to cubemaps.
        allocated_image.storage_image_view = allocated_image.image_view;

        if ( mip_levels > 1 ) {
            // Need to build views per mip, useful for writing
            for ( uint32_t mip = 0; mip < mip_levels; mip++ ) {
                VkImageViewType image_view_type = ( image_type == VK_IMAGE_TYPE_2D )
                    ? VK_IMAGE_VIEW_TYPE_2D
                    : VK_IMAGE_VIEW_TYPE_3D;

                VkImageViewCreateInfo image_view_info
                    = vk::create::image_view_info( format, allocated_image.image, image_view_type,
                        format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                       : VK_IMAGE_ASPECT_COLOR_BIT );

                image_view_info.subresourceRange.baseMipLevel = mip;
                image_view_info.subresourceRange.levelCount = 1;

                VkImageView mip_view;
                vk::check( vkCreateImageView( vulkan.device, &image_view_info, nullptr, &mip_view ),
                    "Failed to create image view" );

                allocated_image.mip_levels.push_back( mip_view );
                vulkan.destructor_stack.push( vulkan.device, mip_view, vkDestroyImageView );
            }
        }

        vulkan.destructor_stack.push(
            vulkan.device, allocated_image.image_view, vkDestroyImageView );
    }

    return allocated_image;
}

vk::mem::AllocatedImage allocate_image( vk::Common& vulkan, VkExtent3D extent, VkFormat format,
    VkImageType image_type, uint32_t mip_levels, uint32_t array_layers,
    VkSampleCountFlagBits samples, VkImageUsageFlags usage_flags, bool mipmapped )
{
    uint32_t sent_mips = mip_levels;

    // Currently, the auto_generate == 0. I'm going under a wild assumption that
    // if you ARE enabling mipmapping, and you pass in 0 mip_levels, then we'll just auto-generate.
    if ( mipmapped && mip_levels == static_cast<uint32_t>( MIP_TYPE::AUTO_GENERATE ) ) {
        sent_mips = static_cast<uint32_t>(
                        std::floor( std::log2( std::max( extent.width, extent.height ) ) ) )
            + 1;
    }

    return allocate_vma_image(
        vulkan, extent, format, image_type, sent_mips, array_layers, samples, usage_flags );
}

std::vector<float> load_image_to_float( const std::string& global_path )
{
    int width, height, channels;
    float* pixels = stbi_loadf( global_path.c_str(), &width, &height, &channels, 4 );

    std::vector<float> float_data(
        static_cast<uint32_t>( width ) * static_cast<uint32_t>( height ) * 4U );
    for ( size_t i = 0; i < static_cast<size_t>( width * height * 4 ); i++ ) {
        float_data[i] = pixels[i];
    }

    stbi_image_free( pixels );
    return float_data;
}

std::vector<uint16_t> load_image_to_float16( const std::string& global_path )
{
    int width, height, channels;
    float* pixels = stbi_loadf( global_path.c_str(), &width, &height, &channels, 4 );

    /// TODO: error checking
    std::vector<uint16_t> half_data(
        static_cast<uint32_t>( width ) * static_cast<uint32_t>( height ) * 4U );
    for ( size_t i = 0; i < static_cast<size_t>( width * height * 4 ); i++ ) {
        half_data[i] = vk::utility::float_to_half( pixels[i] );
    }

    stbi_image_free( pixels );
    return half_data;
}

vk::mem::AllocatedImage load_image( std::filesystem::path file_path, vk::Common& vulkan,
    engine::State& engine, size_t desired_channels, VkFormat image_format )
{
    std::string abs_file_path = std::filesystem::absolute( file_path ).string();

    int width, height, channels;
    unsigned char* pixels = stbi_load( abs_file_path.c_str(), &width, &height, &channels, 4 );

    // if ( static_cast<size_t>( channels ) < desired_channels ) {
    //     log::warn( "[IMAGE LOADER] Channels read doesn't match channels desired!" );
    //     return {};
    // }

    /// TODO: We only support 8bit and 16bit currently.
    // Naive way to find the format type in terms of bits

    FormatType type = FormatType::UNORM8;

    switch ( image_format ) {
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8B8A8_UNORM:
        type = FormatType::UNORM8;
        break;
    case VK_FORMAT_R16G16_SFLOAT:
        type = FormatType::FLOAT16;
        break;
    default:
        break;
    }

    size_t total_pixels = static_cast<size_t>( width * height );

    bool is_mipmapped = false;

    if ( type == FormatType::UNORM8 ) {
        std::vector<uint8_t> byte_data( total_pixels * desired_channels );

        for ( size_t i = 0; i < total_pixels; i++ ) {
            for ( size_t channel = 0; channel < desired_channels; channel++ ) {
                byte_data[i * desired_channels + channel] = pixels[i * 4 + channel];
            }
        }

        stbi_image_free( pixels );

        return engine::create_image( vulkan, engine, byte_data.data(),
            { static_cast<uint32_t>( width ), static_cast<uint32_t>( height ), 1 }, image_format,
            VK_IMAGE_TYPE_2D, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            is_mipmapped );
    }

    if ( type == FormatType::FLOAT16 ) {
        std::vector<uint16_t> half_data( total_pixels * desired_channels );

        for ( size_t i = 0; i < total_pixels; i++ ) {
            for ( size_t channel = 0; channel < desired_channels; channel++ ) {
                uint16_t value = vk::utility::float_to_half( pixels[i * 4 + channel] / 255.0f );
                half_data[i * desired_channels + channel] = value;
            }
        }

        stbi_image_free( pixels );

        return engine::create_image( vulkan, engine, half_data.data(),
            { static_cast<uint32_t>( width ), static_cast<uint32_t>( height ), 1 }, image_format,
            VK_IMAGE_TYPE_2D, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            is_mipmapped );
    }

    log::warn( "[IMAGE LOADER] Desired format currently not supported!" );
    return {};
}

} // namespace racecar::engine
