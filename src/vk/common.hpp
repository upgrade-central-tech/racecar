#pragma once

#include "vma.hpp"

#include <SDL3/SDL.h>
#include <volk.h>
#include <VkBootstrap.h>

#include <optional>

/// Custom define based on VK_CHECK, just with our SDL_Log. Feel free to tweak this.
#define RACECAR_VK_CHECK( vk_fn, message )                              \
    do {                                                                \
        VkResult result = vk_fn;                                        \
        if ( result != VK_SUCCESS ) {                                   \
            SDL_Log( "[Vulkan] %s | Error code: %d", message, result ); \
            return {};                                                  \
        }                                                               \
    } while ( 0 )

namespace racecar::vk {

/// Binding defines.
namespace binding {

constexpr int VERTEX_BUFFER = 0;

}

/// Stores common Vulkan-related objects.
struct Common {
    vkb::Instance instance;
    vkb::Device device;
    VkSurfaceKHR surface = nullptr;

    uint32_t graphics_queue_family = 0;
    VkQueue graphics_queue = nullptr;

    VmaAllocator allocator;
};

std::optional<Common> initialize( SDL_Window* window );
void free( Common& vulkan );

}  // namespace racecar::vk
