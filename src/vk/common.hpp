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
#include "ray_tracing.hpp"

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

namespace GeeometryClass {

constexpr int CAR_ID = 1;
constexpr int TERRAIN_ID = 2;

}

struct GlobalSamplers {
    VkSampler linear_sampler = VK_NULL_HANDLE;
    VkSampler nearest_sampler = VK_NULL_HANDLE;
    VkSampler linear_mirrored_repeat_sampler = VK_NULL_HANDLE;
};

// These are for temporal jittering
const static glm::vec2 Jitter16[16] = {
    glm::vec2( -0.25, -0.1666667 ),
    glm::vec2( 0.25, 0.1666667 ),
    glm::vec2( -0.375, 0.3888889 ),
    glm::vec2( 0.125, -0.0555556 ),
    glm::vec2( -0.125, -0.2777778 ),
    glm::vec2( 0.375, 0.2777778 ),
    glm::vec2( -0.4375, 0.0555556 ),
    glm::vec2( 0.0625, 0.4444444 ),
    glm::vec2( -0.3125, -0.3888889 ),
    glm::vec2( 0.1875, -0.2777778 ),
    glm::vec2( -0.1875, 0.0555556 ),
    glm::vec2( 0.3125, 0.1666667 ),
    glm::vec2( -0.46875, 0.3888889 ),
    glm::vec2( 0.03125, -0.1666667 ),
    glm::vec2( -0.21875, 0.2777778 ),
    glm::vec2( 0.28125, 0.0555556 ),
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

    rt::RayTracingProperties ray_tracing_properties;
};

Common initialize( SDL_Window* window );
void free( Common& vulkan );

} // namespace racecar::vk
