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

DescriptorSet generate_descriptor_set( vk::Common& vulkan, const engine::State& engine,
    const std::vector<VkDescriptorType>& types, VkShaderStageFlags shader_stage_flags );

template <typename UBData>
void update_descriptor_set_uniform( vk::Common& vulkan, const State& engine,
    DescriptorSet& desc_set, UniformBuffer<UBData> uniform_buffer, int binding_idx )
{
    VkBuffer buffer = uniform_buffer.buffer( engine.get_frame_index() ).handle;

    for ( size_t i = 0; i < engine.frame_overlap; ++i ) {
        VkDescriptorBufferInfo buffer_info = {
            .buffer = buffer,
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

void update_descriptor_set_image( vk::Common& vulkan, State& engine, DescriptorSet& desc_set,
    vk::mem::AllocatedImage img, int binding_idx );

void update_descriptor_set_write_image( vk::Common& vulkan, State& engine, DescriptorSet& desc_set,
    vk::mem::AllocatedImage img, int binding_idx );

void update_descriptor_set_rwimage(
    vk::Common& vulkan, State& engine, DescriptorSet& desc_set, RWImage rw_img, int binding_idx );

void update_descriptor_set_sampler( vk::Common& vulkan, State& engine, DescriptorSet& desc_set,
    VkSampler sampler, int binding_idx );

} // namespace racecar::engine
