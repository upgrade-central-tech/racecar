#pragma once

#include "../vk/common.hpp"
#include "state.hpp"

#include <volk.h>

#include <vector>

namespace racecar::engine {

struct DescriptorSet {
    std::vector<VkDescriptorSet> descriptor_sets;
    std::vector<VkDescriptorSetLayout> layouts;
};

DescriptorSet generate_descriptor_set( vk::Common& vulkan, const engine::State& engine,
    const std::vector<VkDescriptorType>& types, VkShaderStageFlags shader_stage_flags );

void update_descriptor_set_uniform( vk::Common& vulkan, State& engine, DescriptorSet& desc_set,
    VkBuffer handle, int binding_idx, VkDeviceSize range );

void update_descriptor_set_image( vk::Common& vulkan, State& engine, DescriptorSet& desc_set,
    vk::mem::AllocatedImage img, int binding_idx );

} // namespace racecar::engine
