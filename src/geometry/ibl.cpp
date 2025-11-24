#include "ibl.hpp"

#include "../engine/descriptor_set.hpp"
#include "../engine/images.hpp"
#include "../engine/pipeline.hpp"
#include "../log.hpp"
#include "../vk/create.hpp"
#include "../vk/utility.hpp"

namespace racecar::geometry {

glm::vec3 cubemap_direction( uint32_t face, float u, float v )
{
    switch ( face ) {
    case 0:
        return glm::normalize( glm::vec3( 1.0f, -v, -u ) );
    case 1:
        return glm::normalize( glm::vec3( -1.0f, -v, u ) );
    case 2:
        return glm::normalize( glm::vec3( u, 1.0f, v ) );
    case 3:
        return glm::normalize( glm::vec3( u, -1.0f, -v ) );
    case 4:
        return glm::normalize( glm::vec3( u, -v, 1.0f ) );
    case 5:
        return glm::normalize( glm::vec3( -u, -v, -1.0f ) );
    }
    return glm::vec3( -1.0f );
}

vk::mem::AllocatedImage generate_diffuse_irradiance(
    std::filesystem::path file_path, vk::Common& vulkan, engine::State& engine )
{
    // Parse the cubemap for each face individually. Somehow log important info?
    const size_t layer_count = 6;
    uint32_t tile_width = 256;
    uint32_t tile_height = 256;
    VkExtent3D tile_extent = { tile_width, tile_height, 1 };

    vk::mem::AllocatedImage cubemap_image
        = allocate_cube_map( vulkan, tile_extent, VK_FORMAT_R32G32B32A32_SFLOAT, 1 );

    vk::mem::AllocatedImage irradiance_rw_image
        = allocate_cube_map( vulkan, tile_extent, VK_FORMAT_R32G32B32A32_SFLOAT, 1 );

    {
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
            face_data[tile] = engine::load_image_to_float( faces[tile] );
        }

        // Need to upload all of these
        // Batched upload necessary. I can't use my brain right now to use our API effectively
        load_cubemap(
            vulkan, engine, face_data, cubemap_image, tile_extent, VK_FORMAT_R32G32B32A32_SFLOAT );
    }
    {
        engine::DescriptorSet prefilter_desc_set = engine::generate_descriptor_set( vulkan, engine,
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
            VK_SHADER_STAGE_COMPUTE_BIT );

        engine::DescriptorSet sampler_desc_set = engine::generate_descriptor_set(
            vulkan, engine, { VK_DESCRIPTOR_TYPE_SAMPLER }, VK_SHADER_STAGE_COMPUTE_BIT );

        VkSampler sampler = VK_NULL_HANDLE;
        {
            VkSamplerCreateInfo sampler_info = {
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter = VK_FILTER_LINEAR,
                .minFilter = VK_FILTER_LINEAR,
                .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            };
            vk::check( vkCreateSampler( vulkan.device, &sampler_info, nullptr, &sampler ),
                "Failed to create irradiance filtering sampler" );
            vulkan.destructor_stack.push( vulkan.device, sampler, vkDestroySampler );
        }

        engine::update_descriptor_set_image( vulkan, engine, prefilter_desc_set, cubemap_image, 0 );
        engine::update_descriptor_set_write_image(
            vulkan, engine, prefilter_desc_set, irradiance_rw_image, 1 );

        engine::update_descriptor_set_sampler( vulkan, engine, sampler_desc_set, sampler, 0 );

        VkShaderModule irradiance_module
            = vk::create::shader_module( vulkan, "../shaders/prefilter/irradiance.spv" );

        std::vector<engine::DescriptorSet> descs = { prefilter_desc_set, sampler_desc_set };

        engine::Pipeline compute_pipeline
            = engine::create_compute_pipeline( vulkan, { descs[0].layouts[0], descs[1].layouts[0] },
                irradiance_module, "cs_compute_irradiance" );

        engine::immediate_submit(
            vulkan, engine.immediate_submit, [&]( VkCommandBuffer command_buffer ) {
                // RW cubemap transition first
                vk::utility::transition_image( command_buffer, irradiance_rw_image.image,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_NONE,
                    VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT );

                vkCmdBindPipeline(
                    command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline.handle );

                VkDescriptorSet sets[] = { prefilter_desc_set.descriptor_sets[0],
                    sampler_desc_set.descriptor_sets[0] };

                vkCmdBindDescriptorSets( command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    compute_pipeline.layout, 0, 2, sets, 0, nullptr );

                vkCmdDispatch( command_buffer, tile_width, tile_height, 6 );

                vk::utility::transition_image( command_buffer, irradiance_rw_image.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT );
            } );
    }

    return irradiance_rw_image;
}

vk::mem::AllocatedImage cs_generate_diffuse_sh( vk::mem::AllocatedImage sample_cubemap,
    VkSampler sampler, vk::Common& vulkan, engine::State& engine )
{
    // Hardcode them to be 9 coefficients for now.
    std::vector<glm::vec3> SH_coefficients( 9, glm::vec3( 0.0f ) );

    // Allocate the coefficeints.
    vk::mem::AllocatedImage sh_coefficients_image = engine::allocate_image( vulkan, { 9, 6, 1 },
        VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TYPE_2D, 1, 1, VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, false );

    {
        engine::DescriptorSet sh_projection_desc0_set = engine::generate_descriptor_set( vulkan,
            engine, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
            VK_SHADER_STAGE_COMPUTE_BIT );

        engine::DescriptorSet sh_projection_desc1_set = engine::generate_descriptor_set(
            vulkan, engine, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE }, VK_SHADER_STAGE_COMPUTE_BIT );

        engine::DescriptorSet sampler_desc_set = engine::generate_descriptor_set(
            vulkan, engine, { VK_DESCRIPTOR_TYPE_SAMPLER }, VK_SHADER_STAGE_COMPUTE_BIT );

        engine::update_descriptor_set_image(
            vulkan, engine, sh_projection_desc0_set, sample_cubemap, 0 );

        engine::update_descriptor_set_sampler( vulkan, engine, sampler_desc_set, sampler, 0 );

        engine::update_descriptor_set_write_image(
            vulkan, engine, sh_projection_desc1_set, sh_coefficients_image, 0 );

        VkShaderModule irradiance_module
            = vk::create::shader_module( vulkan, "../shaders/prefilter/irradiance_sh.spv" );

        std::vector<engine::DescriptorSet> descs
            = { sh_projection_desc0_set, sampler_desc_set, sh_projection_desc1_set };

        std::vector<VkDescriptorSet> bind_descs = {
            sh_projection_desc0_set.descriptor_sets[0],
            sampler_desc_set.descriptor_sets[0],
            sh_projection_desc1_set.descriptor_sets[0],
        };

        engine::Pipeline compute_pipeline = engine::create_compute_pipeline( vulkan,
            { descs[0].layouts[0], descs[1].layouts[0], descs[2].layouts[0] }, irradiance_module,
            "cs_compute_irradiance_sh" );

        engine::immediate_submit(
            vulkan, engine.immediate_submit, [&]( VkCommandBuffer command_buffer ) {
                // RW cubemap transition first
                vk::utility::transition_image( command_buffer, sh_coefficients_image.image,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_NONE,
                    VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT );

                vkCmdBindPipeline(
                    command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline.handle );

                VkDescriptorSet sets[] = { sh_projection_desc0_set.descriptor_sets[0],
                    sampler_desc_set.descriptor_sets[0],
                    sh_projection_desc1_set.descriptor_sets[0] };

                vkCmdBindDescriptorSets( command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    compute_pipeline.layout, 0, 3, sets, 0, nullptr );

                // uint32_t x_groups
                //     = ( static_cast<uint32_t>( sample_cubemap.image_extent.width ) + 7 ) / 8;
                // uint32_t y_groups
                //     = ( static_cast<uint32_t>( sample_cubemap.image_extent.height ) + 7 ) / 8;

                vkCmdDispatch( command_buffer, 1, 1, 1 );

                vk::utility::transition_image( command_buffer, sh_coefficients_image.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT );
            } );
    }

    return sh_coefficients_image;
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
        face_data[tile] = engine::load_image_to_float( faces[tile] );
    }

    std::vector<glm::vec3> sh_coefficients( 9, glm::vec3( 0.0f ) );

    // SH prefiltering... this is a bit confusing, so I'll just follow Ravi's integral
    // key idea is to find L_{lm}, s.t it's an integral over the sphere where:
    // - L(theta, phi) is a pixel value in the environment map,
    // - Y_{lm} are weighted SH functions, already computed/derived for us

    std::vector<std::vector<glm::vec3>> face_directions = {
        { glm::vec3( 1, 0, 0 ), glm::vec3( 0, 0, -1.0f ), glm::vec3( 0, -1, 0 ) }, // px
        { glm::vec3( -1, 0, 0 ), glm::vec3( 0, 0, 1.0f ), glm::vec3( 0, -1, 0 ) }, // nx
        { glm::vec3( 0, 1, 0 ), glm::vec3( 1.0f, 0, 0 ), glm::vec3( 0, 0, 1.0f ) }, // py
        { glm::vec3( 0, -1, 0 ), glm::vec3( 1.0f, 0, 0 ), glm::vec3( 0, 0, -1 ) }, // ny
        { glm::vec3( 0, 0, 1 ), glm::vec3( 1.0f, 0, 0 ), glm::vec3( 0, -1, 0 ) }, // pz
        { glm::vec3( 0, 0, -1 ), glm::vec3( 1.0f, 0, 0.0f ), glm::vec3( 0, -1.0f, 0 ) }, // nz
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
                    uint32_t uindex = static_cast<uint32_t>( index );

                    sh_coefficients[uindex]
                        += ( color * vk::utility::eval_SH( uindex, direction ) * solid_angle );
                }
            }
        }
    }

    return sh_coefficients;
}

vk::mem::AllocatedImage allocate_cube_map(
    vk::Common& vulkan, VkExtent3D extent, VkFormat format, uint32_t mip_levels )
{
    vk::mem::AllocatedImage allocated_image = {
        .image_extent = extent,
        .image_format = format,
    };

    VkImageCreateInfo image_info
        = vk::create::image_info( format, VK_IMAGE_TYPE_2D, mip_levels, 6, VK_SAMPLE_COUNT_1_BIT,
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

        vulkan.destructor_stack.push(
            vulkan.device, allocated_image.image_view, vkDestroyImageView );
    }

    {
        VkImageViewCreateInfo storage_image_view_info = { 
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = allocated_image.image,

            .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
            .format = format,

            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = std::max(mip_levels, 1U),
                .baseArrayLayer = 0,
                .layerCount = 6,
            }, 
        };
        storage_image_view_info.subresourceRange.levelCount = image_info.mipLevels;

        vk::check( vkCreateImageView( vulkan.device, &storage_image_view_info, nullptr,
                       &allocated_image.storage_image_view ),
            "Failed to create storage image view" );

        vulkan.destructor_stack.push(
            vulkan.device, allocated_image.storage_image_view, vkDestroyImageView );
    }

    vulkan.destructor_stack.push_free_vmaimage( vulkan.allocator, allocated_image );

    return allocated_image;
}

vk::mem::AllocatedImage create_cubemap(
    std::filesystem::path file_path, vk::Common& vulkan, engine::State& engine )
{
    // Parse the cubemap for each face individually. Somehow log important info?
    const size_t layer_count = 6;
    uint32_t tile_width = 256;
    uint32_t tile_height = 256;
    VkExtent3D tile_extent = { tile_width, tile_height, 1 };

    vk::mem::AllocatedImage cubemap_image
        = allocate_cube_map( vulkan, tile_extent, VK_FORMAT_R32G32B32A32_SFLOAT, 1 );

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
        face_data[tile] = engine::load_image_to_float( faces[tile] );
    }

    // Need to upload all of these
    // Batched upload necessary. I can't use my brain right now to use our API effectively
    load_cubemap(
        vulkan, engine, face_data, cubemap_image, tile_extent, VK_FORMAT_R32G32B32A32_SFLOAT );

    return cubemap_image;
}

template <typename T>
void load_cubemap( vk::Common& vulkan, engine::State& engine,
    std::vector<std::vector<T>>& face_data, vk::mem::AllocatedImage& cm_image, VkExtent3D extent,
    VkFormat format )
{
    const size_t layer_count = 6;
    const size_t face_size
        = extent.width * extent.height * vk::utility::bytes_from_format( format );
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
                .imageExtent = extent };

            copy_regions[layer].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy_regions[layer].imageSubresource.mipLevel = 0;
            copy_regions[layer].imageSubresource.baseArrayLayer = layer;
            copy_regions[layer].imageSubresource.layerCount = 1;
        }

        engine::immediate_submit(
            vulkan, engine.immediate_submit, [&]( VkCommandBuffer command_buffer ) {
                vk::utility::transition_image( command_buffer, cm_image.image,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_ASPECT_COLOR_BIT );

                vkCmdCopyBufferToImage( command_buffer, upload_buffer.handle, cm_image.image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layer_count, copy_regions.data() );

                vk::utility::transition_image( command_buffer, cm_image.image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT );
            } );

    } catch ( const Exception& ex ) {
        log::error( "[AllocatedImage] Error occurred: {}", ex.what() );
        throw;
    }

    return;
}

}
