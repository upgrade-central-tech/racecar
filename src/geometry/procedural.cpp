#include "procedural.hpp"

#include "../engine/descriptor_set.hpp"
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

vk::mem::AllocatedImage generate_glint_noise( vk::Common& vulkan, engine::State& engine )
{
    uint32_t dim = 512;
    std::vector<float> noise_data( dim * dim * 4 );

    // I think we'd call some compute thing later on to do this.
    return engine::create_image( vulkan, engine, static_cast<void*>( noise_data.data() ),
        { dim, dim, dim }, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TYPE_2D,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false );
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

vk::mem::AllocatedImage create_cubemap( [[maybe_unused]] std::filesystem::path file_path,
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
        abs_file_path + "/px.png",
        abs_file_path + "/nx.png",
        abs_file_path + "/py.png",
        abs_file_path + "/ny.png",
        abs_file_path + "/pz.png",
        abs_file_path + "/nz.png",
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

vk::mem::AllocatedImage create_prefiltered_cubemap(
    vk::Common& vulkan, engine::State& engine, int roughness_levels )
{
    uint32_t width = 256;
    uint32_t height = 256;

    [[maybe_unused]] vk::mem::AllocatedImage prefiltered_cubemap
        = allocate_cube_map( vulkan, { width, height, 1 }, VK_FORMAT_R16G16B16A16_SFLOAT,
            static_cast<uint32_t>( roughness_levels ) );

    // Maybe split the sampler and the input image to something else?
    engine::DescriptorSet prefilter_desc = engine::generate_descriptor_set( vulkan, engine,
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
        VK_SHADER_STAGE_COMPUTE_BIT );

    // Replace this when saahil gets to making compute work, that mf
    VkPipelineLayoutCreateInfo compute_pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = prefilter_desc.layouts.data(),
    };

    VkPipelineLayout compute_pipeline_layout;
    vkCreatePipelineLayout(
        vulkan.device, &compute_pipeline_layout_info, nullptr, &compute_pipeline_layout );

    [[maybe_unused]] VkComputePipelineCreateInfo compute_pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .layout = compute_pipeline_layout,
    };

    return prefiltered_cubemap;

    // VkShaderModule computeShader;

    // compute_pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    // compute_pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    // compute_pipeline_info.stage.module = computeShader;
    // compute_pipeline_info.stage.pName = "main";

    // VkPipeline compute_pipeline;
    // vkCreateComputePipelines( vulkan.device, VK_NULL_HANDLE, 1, &compute_pipeline_info, nullptr,
    // &compute_pipeline);

    // for ( int mip_level = 0; mip_level < roughness_levels; mip_level++ ) {
    //     uint32_t mip_width = width >> mip_level;
    //     uint32_t mip_height = height >> mip_level;

    //     float roughness
    //         = static_cast<float>( mip_level ) / static_cast<float>( roughness_levels - 1 );
    // }
}

vk::mem::AllocatedImage load_image( std::filesystem::path file_path, vk::Common& vulkan,
    engine::State& engine, int desired_channels, VkFormat image_format )
{
    vk::mem::AllocatedImage allocated_image;
    std::string abs_file_path = std::filesystem::absolute( file_path ).string();

    int width, height, channels;
    unsigned char* pixels = stbi_load( abs_file_path.c_str(), &width, &height, &channels, 4 );

    /// TODO: error checking
    /// TODO: This only supports 16bit floats! this needs to be extended much furhter
    ///       to support loading of any arbitrary formats.
    std::vector<uint16_t> half_data( static_cast<uint32_t>( width * height * desired_channels ) );

    for ( int i = 0; i < width * height; i++ ) {
        float r = pixels[i * 4] / 255.0f;
        float g = pixels[i * 4 + 1] / 255.0f;
        [[maybe_unused]] float b = pixels[i * 4 + 2] / 255.0f;
        [[maybe_unused]] float a = pixels[i * 4 + 3] / 255.0f;

        half_data[static_cast<size_t>( i * desired_channels )] = vk::utility::float_to_half( r );

        if ( desired_channels > 1 ) {
            half_data[static_cast<size_t>( i * desired_channels + 1 )]
                = vk::utility::float_to_half( g );
        }
        if ( desired_channels > 2 ) {
            half_data[static_cast<size_t>( i * desired_channels + 2 )]
                = vk::utility::float_to_half( b );
        }
        if ( desired_channels > 3 ) {
            half_data[static_cast<size_t>( i * desired_channels + 3 )]
                = vk::utility::float_to_half( a );
        }
    }

    stbi_image_free( pixels );

    // From half_data, upload to GPU
    bool is_mipmapped = false;
    allocated_image = engine::create_image( vulkan, engine, half_data.data(),
        { static_cast<uint32_t>( width ), static_cast<uint32_t>( height ), 1 }, image_format,
        VK_IMAGE_TYPE_2D, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        is_mipmapped );

    return allocated_image;
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

std::vector<glm::vec3> generate_diffuse_sh( std::filesystem::path file_path )
{
    // Same funcs as the cube map creator
    const size_t layer_count = 6;
    uint32_t tile_width = 256;
    uint32_t tile_height = 256;

    // hardcode file paths for now because screw you
    std::string abs_file_path = std::filesystem::absolute( file_path ).string();
    std::vector<std::string> faces = {
        abs_file_path + "/px.png",
        abs_file_path + "/nx.png",
        abs_file_path + "/py.png",
        abs_file_path + "/ny.png",
        abs_file_path + "/pz.png",
        abs_file_path + "/nz.png",
    };

    // Parse the data somehow
    std::vector<std::vector<float>> face_data( layer_count );
    for ( uint32_t tile = 0; tile < layer_count; tile++ ) {
        face_data[tile] = load_image_to_float( faces[tile] );
    }

    std::vector<glm::vec3> sh_coefficients( 9, glm::vec3( 0.0f ) );

    // SH prefiltering... this is a bit confusing, so I'll just follow Ravi's integral
    // key idea is to find L_{lm}, s.t it's an integral over the sphere where:
    // - L(theta, phi) is a pixel value in the environment map,
    // - Y_{lm} are weighted SH functions, already computed/derived for us

    std::vector<std::vector<glm::vec3>> face_directions = {
        { glm::vec3( -1, 0, 0 ), glm::vec3( 0, 0, 1.0f ), glm::vec3( 0, -1, 0 ) }, // nx
        { glm::vec3( 0, -1, 0 ), glm::vec3( 1.0f, 0, 0 ), glm::vec3( 0, 0, -1 ) }, // ny
        { glm::vec3( 0, 0, -1 ), glm::vec3( 1.0f, 0, 0.0f ), glm::vec3( 0, -1.0f, 0 ) }, // nz
        { glm::vec3( 1, 0, 0 ), glm::vec3( 0, 0, -1.0f ), glm::vec3( 0, -1, 0 ) }, // px
        { glm::vec3( 0, 1, 0 ), glm::vec3( 1.0f, 0, 0 ), glm::vec3( 0, 0, 1.0f ) }, // py
        { glm::vec3( 0, 0, 1 ), glm::vec3( 1.0f, 0, 0 ), glm::vec3( 0, -1, 0 ) }, // pz
    };

    for ( uint32_t face = 0; face < 6; face++ ) {
        for ( uint32_t pixel = 0; pixel < tile_width * tile_height; pixel++ ) {
            // Get color of the cubemap, assume 4 channel data result from face_data parse
            glm::vec3 color = glm::vec3( face_data[face][pixel * 4], face_data[face][pixel * 4 + 1],
                face_data[face][pixel * 4 + 2] );

            // Convert cubemap direction
            uint32_t x = pixel % tile_width;
            uint32_t y = pixel / tile_width;

            float u = 2.0f * ( static_cast<float>( x ) + 0.5f ) / static_cast<float>( tile_width )
                - 1.0f;
            float v = 2.0f * ( static_cast<float>( y ) + 0.5f ) / static_cast<float>( tile_height )
                - 1.0f;

            glm::vec3 direction = face_directions[face][0] + face_directions[face][1] * u
                + face_directions[face][2] * v;
            direction = glm::normalize( direction );

            // Calcualte projected solid angle
            // https://www.rorydriscoll.com/2012/01/15/cubemap-texel-solid-angle/
            float inv_size = 1.0f / static_cast<float>( tile_width );
            float x0 = u - inv_size;
            float y0 = v - inv_size;
            float x1 = u + inv_size;
            float y1 = v + inv_size;
            float solid_angle = vk::utility::area_element( x0, y0 )
                - vk::utility::area_element( x0, y1 ) - vk::utility::area_element( x1, y0 )
                + vk::utility::area_element( x1, y1 );

            // Populate coefficienets using a given pixel contribution
            // Go up to third order, so 3^2
            for ( int l = 0; l < 3; l++ ) {
                for ( int m = -l; m <= l; m++ ) {
                    int index = l * ( l + 1 ) + m;
                    uint32_t uindex = static_cast<uint32_t>(index);

                    sh_coefficients[uindex]
                        += ( color * vk::utility::eval_SH( uindex, direction ) * solid_angle );
                }
            }
        }
    }

    // for ( size_t i = 0; i < sh_coefficients.size(); i++ ) {
    //     // Normalize over the surface area of the sphere, 4 * PI...
    //     // I don't want to do anything nasty with figuring out why M_PI isn't here.
    //     sh_coefficients[i] *= 1.0f / ( 4.0f * 3.14159265358979323846f );
    // }

    return sh_coefficients;
}

}
