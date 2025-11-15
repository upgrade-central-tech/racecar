#include "procedural.hpp"

#include "../engine/images.hpp"
#include "../log.hpp"
#include "../vk/create.hpp"
#include "../vk/mem.hpp"
#include "../vk/utility.hpp"

// scene.hpp has the define for STB_IMAGE_IMPLEMENTATION... not sure if this works?
#include "../scene/scene.hpp"
#include "../stb_image.h"

namespace racecar::geometry {

vk::mem::AllocatedImage generate_test_3D( vk::Common& vulkan, engine::State& engine )
{
    const uint32_t block_size = 32;
    const uint32_t dim = 128;

    // Allocate a 4-channel float texture (RGBA)
    std::vector<float> texture_data( dim * dim * dim * 4 );

    for ( uint32_t z = 0; z < dim; ++z ) {
        for ( uint32_t y = 0; y < dim; ++y ) {
            for ( uint32_t x = 0; x < dim; ++x ) {

                float checkered
                    = ( ( x / block_size ) + ( y / block_size ) + ( z / block_size ) ) % 2;

                size_t idx = 4 * ( x + y * dim + z * dim * dim );
                texture_data[idx + 0] = checkered; // R
                texture_data[idx + 1] = 0.0f; // G
                texture_data[idx + 2] = 0.0f; // B
                texture_data[idx + 3] = 1.0f; // A
            }
        }
    }

    // Create the 3D image
    return engine::create_image( vulkan, engine, static_cast<void*>( texture_data.data() ),
        { dim, dim, dim },
        VK_FORMAT_R32G32B32A32_SFLOAT, // 4-channel float
        VK_IMAGE_TYPE_3D, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false );
}

vk::mem::AllocatedImage allocate_cube_map(
    vk::Common& vulkan, VkExtent3D extent, VkFormat format, uint32_t mip_levels )
{
    vk::mem::AllocatedImage allocated_image = {
        .image_extent = extent,
        .image_format = format,
    };

    VkImageCreateInfo image_info = vk::create::image_info( format, VK_IMAGE_TYPE_2D, mip_levels, 6,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
        extent );

    image_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    {
        VmaAllocationCreateInfo allocation_create_info = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VkMemoryPropertyFlags( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ),
        };

        vk::check( vmaCreateImage( vulkan.allocator, &image_info, &allocation_create_info,
                       &allocated_image.image, &allocated_image.allocation, nullptr ),
            "[VMA] Failed to create image" );
    }

    {
        VkImageViewCreateInfo image_view_info = { 
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = allocated_image.image,

            .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
            .format = format,

            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = std::max(mip_levels, 1U),
                .baseArrayLayer = 0,
                .layerCount = 6,
            }, 
        };
        image_view_info.subresourceRange.levelCount = image_info.mipLevels;

        vk::check( vkCreateImageView(
                       vulkan.device, &image_view_info, nullptr, &allocated_image.image_view ),
            "Failed to create image view" );
    }

    vulkan.destructor_stack.push( vulkan.device, allocated_image.image_view, vkDestroyImageView );
    vulkan.destructor_stack.push_free_vmaimage( vulkan.allocator, allocated_image );

    return allocated_image;
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

vk::mem::AllocatedImage host_prefilter_cubemap( [[maybe_unused]] std::filesystem::path file_path,
    [[maybe_unused]] vk::Common& vulkan, [[maybe_unused]] engine::State& engine )
{
    // Parse the cubemap for each face individually. Somehow log important info?
    const size_t layer_count = 6;
    uint32_t tile_width = 256;
    uint32_t tile_height = 256;
    VkExtent3D tile_extent = { tile_width, tile_height, 1 };

    vk::mem::AllocatedImage cubemap_image
        = allocate_cube_map( vulkan, tile_extent, VK_FORMAT_R16G16B16A16_SFLOAT, 1 );

    // hardcode file paths for now because screw you
    std::string abs_file_path = std::filesystem::absolute( file_path ).string();

    std::vector<std::string> faces = {
        abs_file_path + "/nx.png",
        abs_file_path + "/ny.png",
        abs_file_path + "/nz.png",
        abs_file_path + "/px.png",
        abs_file_path + "/py.png",
        abs_file_path + "/pz.png",
    };

    // Parse the data somehow
    std::vector<std::vector<uint16_t>> face_data( layer_count );
    for ( uint32_t tile = 0; tile < layer_count; tile++ ) {
        face_data[tile] = load_image_to_float16( faces[tile] );
    }

    // Need to upload all of these
    // Batched upload necessary. I can't use my brain right now to use our API effectively
    const size_t face_size = tile_width * tile_height
        * vk::utility::bytes_from_format( VK_FORMAT_R16G16B16A16_SFLOAT );
    const size_t data_size = face_size * layer_count;

    try {
        vk::mem::AllocatedBuffer upload_buffer = vk::mem::create_buffer(
            vulkan, data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU );

        uint8_t* dst = static_cast<uint8_t*>( upload_buffer.info.pMappedData );

        for ( uint32_t i = 0; i < layer_count; i++ ) {
            std::memcpy( dst + i * face_size, face_data[i].data(), face_size );
        }

        std::vector<VkBufferImageCopy> copy_regions( layer_count );
        for ( uint32_t layer = 0; layer < layer_count; layer++ ) {
            copy_regions[layer] = { .bufferOffset = face_size * layer,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageOffset = { 0, 0, 0 },
                .imageExtent = tile_extent };

            copy_regions[layer].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy_regions[layer].imageSubresource.mipLevel = 0;
            copy_regions[layer].imageSubresource.baseArrayLayer = layer;
            copy_regions[layer].imageSubresource.layerCount = 1;
        }

        engine::immediate_submit(
            vulkan, engine.immediate_submit, [&]( VkCommandBuffer command_buffer ) {
                vk::utility::transition_image( command_buffer, cubemap_image.image,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_ASPECT_COLOR_BIT );

                vkCmdCopyBufferToImage( command_buffer, upload_buffer.handle, cubemap_image.image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layer_count, copy_regions.data() );

                vk::utility::transition_image( command_buffer, cubemap_image.image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT );
            } );

    } catch ( const Exception& ex ) {
        log::error( "[AllocatedImage] Error occurred: {}", ex.what() );
        throw;
    }

    return cubemap_image;
}

}
