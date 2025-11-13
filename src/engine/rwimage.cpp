#include "rwimage.hpp"

#include "../log.hpp"
#include "images.hpp"

#include <SDL3/SDL.h>

namespace racecar::engine {

std::optional<RWImage> create_rwimage( vk::Common& vulkan, const engine::State& engine,
    VkExtent3D extent, VkFormat format, VkImageUsageFlags usage_flags, bool mipmapped )
{
    RWImage rwimage;

    for ( size_t i = 0; i < engine.swapchain_images.size(); ++i ) {
        if ( auto allocated_image_opt
            = allocate_image( vulkan, extent, format, usage_flags, mipmapped );
            !allocated_image_opt ) {
            log::error( "[engine::create_rwimage] Failed to create AllocatedImage {} for "
                        "swapchain",
                i + 1 );
            return {};
        } else {
            rwimage.images.push_back( std::move( allocated_image_opt.value() ) );
        }
    }

    return rwimage;
}

} // namespace racecar::engine
