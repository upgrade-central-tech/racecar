#include "rwimage.hpp"

#include "../log.hpp"
#include "images.hpp"

#include <SDL3/SDL.h>

namespace racecar::engine {

RWImage allocate_rwimage( vk::Common& vulkan, const engine::State& engine, VkExtent3D extent,
    VkFormat format, VkImageType image_type, VkSampleCountFlagBits samples,
    VkImageUsageFlags usage_flags, uint32_t mip_levels, bool mipmapped )
{
    RWImage rwimage;

    try {
        for ( size_t i = 0; i < engine.swapchain_images.size(); ++i ) {
            rwimage.images.push_back( allocate_image( vulkan, extent, format, image_type,
                mip_levels, 1, samples, usage_flags, mipmapped ) );
        }
    } catch ( const Exception& ex ) {
        log::error( "[RWImage] Error occurred: {}", ex.what() );
        throw;
    }

    return rwimage;
}

RWImage create_rwimage( vk::Common& vulkan, const engine::State& engine, VkExtent3D extent,
    VkFormat format, VkImageType image_type, VkSampleCountFlagBits samples,
    VkImageUsageFlags usage_flags )
{
    return allocate_rwimage(
        vulkan, engine, extent, format, image_type, samples, usage_flags, 1, false );
}

RWImage create_gbuffer_image( vk::Common& vulkan, const engine::State& engine, VkFormat format,
    VkSampleCountFlagBits samples )
{
    return engine::create_rwimage( vulkan, engine,
        VkExtent3D( engine.swapchain.extent.width, engine.swapchain.extent.height, 1 ), format,
        VkImageType::VK_IMAGE_TYPE_2D, samples,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );
}

RWImage create_rwimage_mips( vk::Common& vulkan, const engine::State& engine, VkExtent3D extent,
    VkFormat format, VkImageType image_type, VkSampleCountFlagBits samples,
    VkImageUsageFlags usage_flags, uint32_t mip_levels )
{
    return allocate_rwimage(
        vulkan, engine, extent, format, image_type, samples, usage_flags, mip_levels, true );
}

} // namespace racecar::engine
