#include "procedural.hpp"

#include "../engine/descriptor_set.hpp"
#include "../engine/images.hpp"
#include "../log.hpp"
#include "../vk/create.hpp"
#include "../vk/mem.hpp"
#include "../vk/utility.hpp"

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

}
