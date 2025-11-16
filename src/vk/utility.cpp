#include "utility.hpp"

#include "create.hpp"

namespace racecar::vk::utility {

void transition_image( VkCommandBuffer command_buffer, VkImage image, VkImageLayout old_layout,
    VkImageLayout new_layout, VkAccessFlags2 src_access_mask, VkAccessFlags2 dst_access_mask,
    VkPipelineStageFlags2 src_stage_mask, VkPipelineStageFlags2 dst_stage_mask,
    VkImageAspectFlags aspect_flags )
{
    VkImageMemoryBarrier2 image_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

        // For example, we can consider only looking at COMPUTE work, so
        // VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT.
        // Similarly, dstStageMask represents the work that waits until the barrier is
        // lifted.
        //
        // Note: VkGuide notes that ALL_COMMANDS in StageMask is inefficient!!!
        // More refined examples of using StageMask effectively can be found here:
        // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
        .srcStageMask = src_stage_mask,
        .srcAccessMask = src_access_mask,
        .dstStageMask = dst_stage_mask,
        .dstAccessMask = dst_access_mask,

        .oldLayout = old_layout,
        .newLayout = new_layout,

        .image = image,
        .subresourceRange = vk::create::image_subresource_range( aspect_flags ),
    };

    VkDependencyInfo dependency_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &image_barrier,
    };

    vkCmdPipelineBarrier2( command_buffer, &dependency_info );
}

/// Open to ideas for how to replace this.
uint32_t bytes_from_format( VkFormat format )
{
    switch ( format ) {
    case VK_FORMAT_R8_UNORM:
        return 1;
    case VK_FORMAT_R8G8B8_UNORM:
        return 3;
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_R8G8B8A8_UNORM:
        return 4;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return 8;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return 16;

    default:
        return 0;
    }
}

uint16_t float_to_half( float f )
{
    uint32_t x = *(uint32_t*)&f;
    uint16_t sign = ( x >> 16 ) & 0x8000;
    int32_t exponent = ( ( x >> 23 ) & 0xFF ) - 127 + 15;
    uint16_t mantissa = ( x >> 13 ) & 0x3FF;

    if ( exponent <= 0 )
        return sign;
    if ( exponent >= 31 )
        return static_cast<uint16_t>( sign | 0x7C00 ); // overflow
    return static_cast<uint16_t>( sign | ( exponent << 10 ) | mantissa );
}

// Grabbed from Ramamoorthi's paper: https://cseweb.ucsd.edu/~ravir/papers/envmap/envmap.pdf
// These pretty much come for free, though these go up to order 3
float eval_SH( uint32_t lm_index, glm::vec3 direction )
{
    if ( lm_index == 0 ) // 0,  0
        return 0.282095f;
    if ( lm_index == 1 ) // 1, -1
        return 0.488603f * direction.y;
    if ( lm_index == 2 ) // 1,  0
        return 0.488603f * direction.z;
    if ( lm_index == 3 ) // 1,  1
        return 0.488603f * direction.x;
    if ( lm_index == 4 ) // 2, -2
        return 1.092548f * ( direction.x * direction.y );
    if ( lm_index == 5 ) // 2, -1
        return 1.092548f * ( direction.y * direction.z );
    if ( lm_index == 6 ) // 2,  0
        return 0.315392f * ( 3.0f * ( direction.z * direction.z ) - 1.0f );
    if ( lm_index == 7 ) // 2,  1
        return 1.092548f * ( direction.x * direction.z );
    if ( lm_index == 8 ) // 2,  2
        return 0.546274f * ( ( direction.x * direction.x ) - ( direction.y * direction.y ) );
    
    return -1.0f;
}

float area_element( float x, float y ) {
    return atan2( x * y , sqrt( x * x + y * y + 1.0f ) );
}

} // namespace racecar::vk::utility
