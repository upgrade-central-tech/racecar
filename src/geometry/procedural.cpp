#include "procedural.hpp"

#include "../engine/descriptor_set.hpp"
#include "../engine/images.hpp"
#include "../engine/pipeline.hpp"
#include "../stb_image.h"
#include "../vk/create.hpp"
#include "../vk/mem.hpp"
#include "../vk/utility.hpp"

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
    uint32_t noise_texture_size = 512;

    // Allocate the coefficeints.
    vk::mem::AllocatedImage glint_noise_texture = engine::allocate_image( vulkan,
        { noise_texture_size, noise_texture_size, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_TYPE_2D, 1, 1, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, false );

    {
        engine::DescriptorSet glint_desc_set = engine::generate_descriptor_set(
            vulkan, engine, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE }, VK_SHADER_STAGE_COMPUTE_BIT );

        engine::update_descriptor_set_write_image(
            vulkan, engine, glint_desc_set, glint_noise_texture, 0 );

        VkShaderModule glint_noise_init_module
            = vk::create::shader_module( vulkan, "../shaders/glint/glint_noise_init.spv" );

        std::vector<engine::DescriptorSet> descs = { glint_desc_set };

        std::vector<VkDescriptorSet> bind_descs = { glint_desc_set.descriptor_sets[0] };

        engine::Pipeline compute_pipeline = engine::create_compute_pipeline(
            vulkan, { descs[0].layouts[0] }, glint_noise_init_module, "cs_generate_glint_noise" );

        engine::immediate_submit(
            vulkan, engine.immediate_submit, [&]( VkCommandBuffer command_buffer ) {
                // RW cubemap transition first
                vk::utility::transition_image( command_buffer, glint_noise_texture.image,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_NONE,
                    VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT );

                vkCmdBindPipeline(
                    command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline.handle );

                VkDescriptorSet sets[] = { glint_desc_set.descriptor_sets[0] };

                vkCmdBindDescriptorSets( command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    compute_pipeline.layout, 0, 1, sets, 0, nullptr );

                uint32_t x_groups = ( static_cast<uint32_t>( noise_texture_size ) + 7 ) / 8;
                uint32_t y_groups = ( static_cast<uint32_t>( noise_texture_size ) + 7 ) / 8;

                vkCmdDispatch( command_buffer, x_groups, y_groups, 1 );

                vk::utility::transition_image( command_buffer, glint_noise_texture.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT );
            } );
    }

    return glint_noise_texture;
}

}
