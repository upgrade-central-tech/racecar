#pragma once

#include "../vk/common.hpp"
#include "rwimage.hpp"
#include "state.hpp"
#include "uniform_buffer.hpp"

#include <volk.h>

#include <vector>

namespace racecar::engine {

struct DescriptorSet {
    std::vector<VkDescriptorSet> descriptor_sets;
    std::vector<VkDescriptorSetLayout> layouts;
};

DescriptorSet generate_descriptor_set(
    const std::vector<VkDescriptorType>& types, VkShaderStageFlags shader_stage_flags );

DescriptorSet generate_array_descriptor_set(
    const std::vector<VkDescriptorType>& types, VkShaderStageFlags shader_stage_flags, uint32_t count );


template <typename UBData>
void update_descriptor_set_uniform(
    DescriptorSet& desc_set, const UniformBuffer<UBData>& uniform_buffer, int binding_idx )
{
    const vk::Common& vulkan = vk::Common::GetConst();
    const engine::State& engine = engine::State::GetConst();

    for ( size_t i = 0; i < engine.frame_overlap; ++i ) {
        VkDescriptorBufferInfo buffer_info = {
            .buffer = uniform_buffer.buffer( i ).handle,
            .offset = 0,
            .range = sizeof( UBData ),
        };

        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = desc_set.descriptor_sets[i],
            .dstBinding = static_cast<uint32_t>( binding_idx ),
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &buffer_info,
        };

        vkUpdateDescriptorSets( vulkan.device, 1, &write, 0, nullptr );
    }
}

void update_descriptor_set_const_storage_buffer(
    DescriptorSet& desc_set, vk::mem::AllocatedBuffer storage_buffer, int binding_idx );

void update_descriptor_set_storage_buffer_per_frame(
    DescriptorSet& desc_set, const std::vector<vk::mem::AllocatedBuffer>& storage_buffers,
    int binding_idx );

void update_descriptor_set_image( DescriptorSet& desc_set,
    const vk::mem::AllocatedImage& img, int binding_idx );

void update_descriptor_set_image_array( DescriptorSet& desc_set,
    const std::vector<vk::mem::AllocatedImage>& imgs, int binding_idx );

void update_descriptor_set_rwimage_array( DescriptorSet& desc_set,
    const std::vector<const RWImage*>& imgs, VkImageLayout img_layout, int binding_idx,
    uint32_t array_count );

void update_descriptor_set_depth_image( DescriptorSet& desc_set,
    const RWImage& depth_img, int binding_idx );

void update_descriptor_set_write_image( DescriptorSet& desc_set,
    const vk::mem::AllocatedImage& img, int binding_idx );

void update_descriptor_set_rwimage(
    DescriptorSet& desc_set, const RWImage& rw_img, VkImageLayout img_layout, int binding_idx );

void update_descriptor_set_rwimage_mip(
    DescriptorSet& desc_set, const RWImage& rw_img, VkImageLayout img_layout, int binding_idx,
    size_t mip );

void update_descriptor_set_sampler(
    DescriptorSet& desc_set, VkSampler sampler, int binding_idx );

#if RACECAR_RAY_TRACING
void update_descriptor_set_acceleration_structure(
    DescriptorSet& desc_set, VkAccelerationStructureKHR tlas, int binding_idx );

void update_descriptor_set_acceleration_structure_per_frame(
    DescriptorSet& desc_set, const std::vector<VkAccelerationStructureKHR>& tlases,
    int binding_idx );
#endif // RACECAR_RAY_TRACING

} // namespace racecar::engine
