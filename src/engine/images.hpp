#pragma once

#include "../vk/common.hpp"
#include "state.hpp"

namespace racecar::engine {

struct SamplersDescriptor {
    VkDescriptorSetLayout layout;
    std::vector<VkDescriptorSet> descriptor_sets;
    std::vector<std::vector<VkSampler>> samplers;
};

SamplersDescriptor create_samplers_descriptor( vk::Common& vulkan,
                                               engine::State& engine,
                                               const std::vector<VkSampler>& samplers,
                                               VkShaderStageFlags shader_flags,
                                               uint32_t frame_overlap );

struct ImagesDescriptor {
    VkDescriptorSetLayout layout;
    std::vector<VkDescriptorSet> descriptor_sets;
    std::vector<std::vector<vk::mem::AllocatedImage>> images;
};

ImagesDescriptor create_images_descriptor( vk::Common& vulkan,
                                           engine::State& engine,
                                           const std::vector<vk::mem::AllocatedImage>& images,
                                           VkShaderStageFlags shader_flags,
                                           uint32_t frame_overlap );

void update_images( vk::Common& vulkan,
                    engine::ImagesDescriptor& image_descriptors,
                    std::vector<vk::mem::AllocatedImage>& images,
                    int32_t frame_index );

std::optional<vk::mem::AllocatedImage> create_image( vk::Common& vulkan,
                                                     engine::State& engine,
                                                     void* data,
                                                     VkExtent3D size,
                                                     VkFormat format,
                                                     VkImageUsageFlags usage,
                                                     bool mipmapped );

std::optional<vk::mem::AllocatedImage> allocate_image( vk::Common& vulkan,
                                                       VkExtent3D size,
                                                       VkFormat format,
                                                       VkImageUsageFlags usage,
                                                       bool mipmapped );

std::optional<vk::mem::AllocatedImage> create_allocated_image( vk::Common& vulkan,
                                                               engine::State& engine,
                                                               void* data,
                                                               VkExtent3D size,
                                                               VkFormat format,
                                                               VkImageUsageFlags usage,
                                                               bool mipmapped );

void destroy_image( vk::Common& vulkan, const vk::mem::AllocatedImage& image );

}  // namespace racecar::engine
