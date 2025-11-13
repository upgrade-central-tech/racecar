#pragma once

#include "../engine/destructor_stack.hpp"
#include "../exception.hpp"
#include "../log.hpp"
#include "vma.hpp"

#include <SDL3/SDL_video.h>
#include <volk.h>
#include <VkBootstrap.h>

#include <source_location>
#include <string_view>

/// Custom define based on VK_CHECK.
#define RACECAR_VK_CHECK( vk_fn, message )                                                         \
    do {                                                                                           \
        if ( VkResult result = vk_fn; result ) {                                                   \
            log::error( "[Vulkan] {}, error code: {}", message, static_cast<int>( result ) );      \
            return {};                                                                             \
        }                                                                                          \
    } while ( 0 )

namespace racecar::vk {

inline constexpr void check( VkResult result, std::string_view message,
    const std::source_location& loc = std::source_location::current() )
{
    if ( result ) {
        throw Exception( "[vk] [{}({}:{})] {} (code {})", loc.file_name(), loc.line(), loc.column(),
            message, static_cast<int>( result ) );
    }
}

/// Binding defines.
namespace binding {

constexpr int VERTEX_BUFFER = 0;
constexpr int MAX_IMAGES_BINDED = 4;

} // namespace binding

/// Stores common Vulkan-related objects.
struct Common {
    vkb::Instance instance;
    vkb::Device device;
    VkSurfaceKHR surface = nullptr;

    uint32_t graphics_queue_family = 0;
    VkQueue graphics_queue = nullptr;

    VmaAllocator allocator;

    DestructorStack destructor_stack;
};

Common initialize( SDL_Window* window );
void free( Common& vulkan );

} // namespace racecar::vk
