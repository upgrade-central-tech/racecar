#include "image.h"

#include "../log.hpp"
#include "../vk/create.hpp"
#include "../vk/utility.hpp"
#include "imm_submit.hpp"

#include <stb_image.h>

namespace racecar::engine {

namespace {

enum class FormatType { UNORM8, FLOAT16 };
enum class MIP_TYPE : uint32_t { AUTO_GENERATE = 0 };

void generate_mipmaps(
    VkImage image, VkExtent3D extent, uint32_t mip_levels, VkCommandBuffer cmd_buffer
)
{
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,

        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    int32_t width = static_cast<int32_t>( extent.width );
    int32_t height = static_cast<int32_t>( extent.height );

    for ( uint32_t mip = 1; mip < mip_levels; mip++ ) {
        barrier.subresourceRange.baseMipLevel = mip - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(
            cmd_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier
        );

        VkImageBlit blit = {
            .srcSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = mip - 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            .dstSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = mip,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };

        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { width, height, 1 };

        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { width > 1 ? width / 2 : 1, height > 1 ? height / 2 : 1, 1 };

        vkCmdBlitImage(
            cmd_buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &blit,
            VK_FILTER_LINEAR
        );

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            cmd_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier
        );

        if ( width > 1 ) {
            width /= 2;
        }
        if ( height > 1 ) {
            height /= 2;
        }
    }

    barrier.subresourceRange.baseMipLevel = mip_levels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        cmd_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier
    );
}

Image allocate_vma_image(
    VkExtent3D extent,
    VkFormat format,
    VkImageType image_type,
    uint32_t mip_levels,
    uint32_t array_layers,
    VkSampleCountFlagBits samples,
    VkImageUsageFlags usage_flags
)
{
    vk::Common& vulkan = vk::Common::GetMut( );
    Image allocated_image;
    allocated_image.image_extent = extent;
    allocated_image.image_format = format;

    VkImageCreateInfo image_info = vk::create::image_info(
        format,
        image_type,
        mip_levels,
        array_layers,
        samples,
        usage_flags,
        extent
    );

    {
        VmaAllocationCreateInfo allocation_create_info = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VkMemoryPropertyFlags( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ),
        };
        vk::check(
            vmaCreateImage(
                vulkan.allocator,
                &image_info,
                &allocation_create_info,
                &allocated_image.vk_image,
                &allocated_image.allocation,
                nullptr
            ),
            "[VMA] Failed to create image"
        );
        vulkan.destructor_stack.push_free_vmaimage(
            vulkan.allocator,
            allocated_image.vk_image,
            allocated_image.allocation
        );
    }

    {
        // Let's just assume there are binary results
        VkImageViewType image_view_type = ( image_type == VK_IMAGE_TYPE_2D )
            ? VK_IMAGE_VIEW_TYPE_2D
            : VK_IMAGE_VIEW_TYPE_3D;

        VkImageViewCreateInfo image_view_info = vk::create::image_view_info(
            format,
            allocated_image.vk_image,
            image_view_type,
            format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT
        );
        image_view_info.subresourceRange.levelCount = image_info.mipLevels;
        vk::check(
            vkCreateImageView(
                vulkan.device,
                &image_view_info,
                nullptr,
                &allocated_image.image_view
            ),
            "Failed to create image view"
        );

        if ( mip_levels > 1 ) {
            // Need to build views per mip, useful for writing
            for ( uint32_t mip = 0; mip < mip_levels; mip++ ) {
                VkImageViewType image_view_type = ( image_type == VK_IMAGE_TYPE_2D )
                    ? VK_IMAGE_VIEW_TYPE_2D
                    : VK_IMAGE_VIEW_TYPE_3D;

                VkImageViewCreateInfo image_view_info = vk::create::image_view_info(
                    format,
                    allocated_image.vk_image,
                    image_view_type,
                    format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                   : VK_IMAGE_ASPECT_COLOR_BIT
                );

                image_view_info.subresourceRange.baseMipLevel = mip;
                image_view_info.subresourceRange.levelCount = 1;

                VkImageView mip_view;
                vk::check(
                    vkCreateImageView( vulkan.device, &image_view_info, nullptr, &mip_view ),
                    "Failed to create image view"
                );

                allocated_image.mip_levels.push_back( mip_view );
                vulkan.destructor_stack.push( vulkan.device, mip_view, vkDestroyImageView );
            }
        }

        vulkan.destructor_stack
            .push( vulkan.device, allocated_image.image_view, vkDestroyImageView );
    }

    return allocated_image;
}

Image allocate_image(
    VkExtent3D extent,
    VkFormat format,
    VkImageType image_type,
    uint32_t mip_levels,
    uint32_t array_layers,
    VkSampleCountFlagBits samples,
    VkImageUsageFlags usage_flags,
    bool mipmapped
)
{
    uint32_t sent_mips = mip_levels;

    // Currently, the auto_generate == 0. I'm going under a wild assumption that
    // if you ARE enabling mipmapping, and you pass in 0 mip_levels, then we'll just auto-generate.
    if ( mipmapped && mip_levels == static_cast<uint32_t>( MIP_TYPE::AUTO_GENERATE ) ) {
        sent_mips = static_cast<uint32_t>(
                        std::floor( std::log2( std::max( extent.width, extent.height ) ) )
                    ) +
            1;
    }

    try {
        return allocate_vma_image(
            extent,
            format,
            image_type,
            sent_mips,
            array_layers,
            samples,
            usage_flags
        );
    } catch ( const Exception& ex ) {
        log::error( "[Image] Error occurred: {}", ex.what( ) );
        throw;
    }
}

} // namespace

Image create_image(
    VkExtent3D extent,
    VkFormat format,
    VkImageType image_type,
    VkSampleCountFlagBits samples,
    VkImageUsageFlags usage_flags
)
{
    return allocate_image( extent, format, image_type, 1, 1, samples, usage_flags, false );
}

Image create_image_mips(
    VkExtent3D extent,
    VkFormat format,
    VkImageType image_type,
    VkSampleCountFlagBits samples,
    VkImageUsageFlags usage_flags,
    uint32_t mip_levels
)
{
    return allocate_image( extent, format, image_type, mip_levels, 1, samples, usage_flags, true );
}

Image create_gbuffer_image( VkFormat format, VkSampleCountFlagBits samples )
{
    const engine::State& engine = engine::State::GetConst( );
    return engine::create_image(
        VkExtent3D( engine.swapchain.extent.width, engine.swapchain.extent.height, 1 ),
        format,
        VkImageType::VK_IMAGE_TYPE_2D,
        samples,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    );
}

Image create_image_from_data(
    void* data,
    VkExtent3D extent,
    VkFormat format,
    VkImageType image_type,
    VkImageUsageFlags usage_flags,
    bool mipmapped
)
{
    const engine::State& engine = engine::State::GetConst( );
    const size_t data_size = extent.depth * extent.width * extent.height *
        vk::utility::bytes_from_format( format );

    Image new_image;

    uint32_t mip_levels = 1;
    if ( mipmapped ) {
        // Auto generate the miplevels
        mip_levels = static_cast<uint32_t>(
                         std::floor( std::log2( std::max( extent.width, extent.height ) ) )
                     ) +
            1;
    }

    try {
        vk::mem::AllocatedBuffer upload_buffer = vk::mem::create_buffer(
            data_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU
        );

        std::memcpy( upload_buffer.info.pMappedData, data, data_size );

        new_image = allocate_image(
            extent,
            format,
            image_type,
            mip_levels,
            1,
            VK_SAMPLE_COUNT_1_BIT,
            usage_flags | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            mipmapped
        );

        engine::immediate_submit( engine.immediate_submit, [&]( VkCommandBuffer command_buffer ) {
            VkImageAspectFlags aspect_flags = format == VK_FORMAT_D32_SFLOAT
                ? VK_IMAGE_ASPECT_DEPTH_BIT
                : VK_IMAGE_ASPECT_COLOR_BIT;

            vk::utility::transition_image_mips(
                command_buffer,
                new_image.vk_image,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                0,
                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                aspect_flags,
                mip_levels
            );

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

            vkCmdCopyBufferToImage(
                command_buffer,
                upload_buffer.handle,
                new_image.vk_image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &copy_region
            );

            if ( mipmapped ) {
                // this function handles the image transitions for each image for us
                generate_mipmaps(
                    new_image.vk_image,
                    new_image.image_extent,
                    mip_levels,
                    command_buffer
                );
            } else {
                vk::utility::transition_image(
                    command_buffer,
                    new_image.vk_image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_ACCESS_2_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    aspect_flags
                );
            }
        } );
    } catch ( const Exception& ex ) {
        log::error( "[Image] Error occurred: {}", ex.what( ) );
        throw;
    }

    return new_image;
}

Image create_image_from_load(
    std::filesystem::path file_path,
    size_t desired_channels,
    VkFormat image_format,
    bool is_mipmapped
)
{
    std::string abs_file_path = std::filesystem::absolute( file_path ).string( );

    int width, height, channels;
    unsigned char* pixels = stbi_load( abs_file_path.c_str( ), &width, &height, &channels, 4 );

    // if ( static_cast<size_t>( channels ) < desired_channels ) {
    //     log::warn( "[IMAGE LOADER] Channels read doesn't match channels desired!" );
    //     return {};
    // }

    /// TODO: We only support 8bit and 16bit currently.
    // Naive way to find the format type in terms of bits

    FormatType type = FormatType::UNORM8;

    switch ( image_format ) {
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
        type = FormatType::UNORM8;
        break;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R16G16_SFLOAT:
        type = FormatType::FLOAT16;
        break;
    default:
        break;
    }

    size_t total_pixels = static_cast<size_t>( width * height );

    if ( type == FormatType::UNORM8 ) {
        std::vector<uint8_t> byte_data( total_pixels * desired_channels );

        for ( size_t i = 0; i < total_pixels; i++ ) {
            for ( size_t channel = 0; channel < desired_channels; channel++ ) {
                byte_data[i * desired_channels + channel] = pixels[i * 4 + channel];
            }
        }

        stbi_image_free( pixels );

        return create_image_from_data(
            byte_data.data( ),
            { static_cast<uint32_t>( width ), static_cast<uint32_t>( height ), 1 },
            image_format,
            VK_IMAGE_TYPE_2D,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            is_mipmapped
        );
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

        return create_image_from_data(
            half_data.data( ),
            { static_cast<uint32_t>( width ), static_cast<uint32_t>( height ), 1 },
            image_format,
            VK_IMAGE_TYPE_2D,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            is_mipmapped
        );
    }

    log::warn( "[IMAGE LOADER] Desired format currently not supported!" );
    return { };
}

void initialize_image_layout( Image& image, VkImageLayout layout )
{
    const engine::State& engine = engine::State::GetConst( );
    engine::immediate_submit( engine.immediate_submit, [&]( VkCommandBuffer command_buffer ) {
        vk::utility::transition_image(
            command_buffer,
            image.vk_image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            layout,
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
    } );
}

} // namespace racecar::engine
