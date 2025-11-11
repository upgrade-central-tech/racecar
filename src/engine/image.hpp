#pragma once

#include "../vk/mem.hpp"
#include "state.hpp"

namespace racecar::image {

bool create_debug_image_data( vk::Common& vulkan, engine::State& engine );
                                                
/// Memory creation.
std::optional<vk::mem::AllocatedImage> allocate_image( vk::Common& vulkan,
                                              VkExtent3D size,
                                              VkFormat format,
                                              VkImageUsageFlags usage,
                                              bool mipmapped = false );

std::optional<vk::mem::AllocatedImage> create_image( vk::Common& vulkan,
                                            engine::State& engine,
                                            void* data,
                                            VkExtent3D size,
                                            VkFormat format,
                                            VkImageUsageFlags usage,
                                            bool mipmapped = false );


void destroy_image( vk::Common& vulkan, const vk::mem::AllocatedImage& image);

}