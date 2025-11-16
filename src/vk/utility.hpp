#pragma once

#include <volk.h>
#include <glm/glm.hpp>

namespace racecar::vk::utility {

/// Similar to a resource barrier transition in DX12, this is meant to adjust a given image's
/// layout/access properties, etc. For example, to write to our image while drawing, we need our
/// image layout to transition from a read only state (VK_IMAGE_LAYOUT_GENERAL) to write
/// (VK_IMAGE_LAYOUT_PRESENT_SRC_KHR).
///
/// Helpful execution and memory barrier info can be found here:
/// https://themaister.net/blog/2019/08/14/yet-another-blog-explaining-vulkan-synchronization
void transition_image( VkCommandBuffer command_buffer, VkImage image, VkImageLayout old_layout,
    VkImageLayout new_layout, VkAccessFlags2 src_access_mask, VkAccessFlags2 dst_access_mask,
    VkPipelineStageFlags2 src_stage_mask, VkPipelineStageFlags2 dst_stage_mask,
    VkImageAspectFlags aspect_flags );

uint32_t bytes_from_format( VkFormat format );

uint16_t float_to_half( float f );
float half_to_float( uint16_t h );

// SH helper funcs
float eval_SH( uint32_t lm_index, glm::vec3 direction );
float area_element( float x, float y );

// Glint helper funcs, later to be removed once compute is setup
uint32_t coord_to_flat( uint32_t coord );

uint32_t wang_hash( uint32_t seed );

uint32_t rand_xor_shift( uint32_t rng_state );

float rand_xor_shift_float( uint32_t rng_state );

float erf( float x );

} // namespace racecar::vk::utility
