#include "procedural.hpp"
#include "../engine/images.hpp"
#include "../vk/mem.hpp"


namespace racecar::geometry {

vk::mem::AllocatedImage generate_test_3D( vk::Common& vulkan, engine::State& engine )
{
    // uint32_t block_size = 8;
    uint32_t dim = 128;

    // 
    std::vector<uint8_t> texture_data( dim * dim * dim );

    for ( uint32_t z = 0; z < dim; z++ ) {
        for ( uint32_t y = 0; y < dim; y++ ) {
            for ( uint32_t x = 0; x < dim; x++ ) { 
                uint8_t black_or_white = 255; //  * ( ( (x / block_size) + ( y / block_size ) + ( z / block_size ) ) % 2 );
                
                size_t index = x + ( y * dim ) + ( z * dim * dim );

                texture_data[index] = black_or_white;
            }
        }
    }

    return engine::create_image( vulkan, engine,
            static_cast<void*>( texture_data.data() ),
            { static_cast<uint32_t>( dim ), static_cast<uint32_t>( dim ), static_cast<uint32_t>( dim ) },
            VK_FORMAT_R8_UNORM, VK_IMAGE_TYPE_3D, VK_IMAGE_USAGE_SAMPLED_BIT, false );
}

}
