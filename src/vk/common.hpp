#pragma once

#include "../engine/destructor_stack.hpp"
#include "../exception.hpp"
#include "vma.hpp"

#include <SDL3/SDL_video.h>
#include <volk.h>

#if RACECAR_MACOS
/// Terrible hack that's needed because of mismatched Vulkan headers. Honestly IDK what's happening,
/// this function is never used (and not even supported), I blame vcpkg for everything
#define fp_vkCmdDispatchTileQCOM( commandBuffer ) fp_vkCmdDispatchTileQCOM( commandBuffer, nullptr )
#endif
#include <VkBootstrap.h>

#include <source_location>
#include <string_view>

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

struct GlobalSamplers {
    VkSampler linear_sampler;
    VkSampler nearest_sampler;
};

/// Stores common Vulkan-related objects.
struct Common {
    vkb::Instance instance;
    vkb::Device device;
    VkSurfaceKHR surface = nullptr;

    uint32_t graphics_queue_family = 0;
    VkQueue graphics_queue = nullptr;

    // Shared static Vk instances
    // 0 : for linear sampler
    // 1 : for nearest sampler
    GlobalSamplers global_samplers;

    VmaAllocator allocator;

    DestructorStack destructor_stack;
};

Common initialize( SDL_Window* window );
void free( Common& vulkan );

} // namespace racecar::vk
