#include "rwimage.hpp"

#include "../log.hpp"
#include "images.hpp"

#include <SDL3/SDL.h>

namespace racecar::engine {

RWImage create_rwimage( vk::Common& vulkan, const engine::State& engine, VkExtent3D extent,
    VkFormat format, VkImageType image_type, VkImageUsageFlags usage_flags, bool mipmapped )
{
    RWImage rwimage;

    try {
        for ( size_t i = 0; i < engine.swapchain_images.size(); ++i ) {
            rwimage.images.push_back( allocate_image(
                vulkan, extent, format, image_type, 1, 1, usage_flags, mipmapped ) );
        }
    } catch ( const Exception& ex ) {
        log::error( "[RWImage] Error occurred: {}", ex.what() );
        throw;
    }

    return rwimage;
}

RWImage create_ms_rwimage( vk::Common& vulkan, const engine::State& engine, VkExtent3D extent,
    VkFormat format, VkImageType image_type, VkSampleCountFlagBits samples, VkImageUsageFlags usage_flags, bool mipmapped )
{
    RWImage rwimage;

    try {
        for ( size_t i = 0; i < engine.swapchain_images.size(); ++i ) {
            rwimage.images.push_back( allocate_ms_image(
                vulkan, extent, format, image_type, 1, 1, samples, usage_flags, mipmapped ) );
        }
    } catch ( const Exception& ex ) {
        log::error( "[RWImage] Error occurred: {}", ex.what() );
        throw;
    }

    return rwimage;
}


} // namespace racecar::engine
