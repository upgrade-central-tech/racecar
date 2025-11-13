#include "descriptor_set.hpp"

#include "descriptors.hpp"
#include "state.hpp"

namespace racecar::engine {

DescriptorSet generate_descriptor_set( vk::Common& vulkan, const engine::State& engine,
    const std::vector<VkDescriptorType>& types, VkShaderStageFlags shader_stage_flags )
{
    const size_t num_frames = engine.swapchain_images.size();

    DescriptorSet desc_set = {
        .handles = std::vector<VkDescriptorSet>( num_frames ),
        .layouts = std::vector<VkDescriptorSetLayout>( num_frames ),
    };

    engine::DescriptorLayoutBuilder builder;

    for ( uint32_t i = 0; i < static_cast<uint32_t>( types.size() ); ++i ) {
        engine::descriptor_layout_builder::add_binding( builder, i, types[i] );
    }

    for ( size_t i = 0; i < num_frames; ++i ) {
        try {
            desc_set.layouts[i]
                = engine::descriptor_layout_builder::build( vulkan, builder, shader_stage_flags );
            desc_set.handles[i] = engine::descriptor_allocator::allocate(
                vulkan, engine.descriptor_system.frame_allocators[i], desc_set.layouts[i] );
        } catch ( const Exception& ex ) {
            log::error( "[DescriptorSet] Failed to generate" );
            throw;
        }
    }

    return desc_set;
}

void update_descriptor_set_uniform( vk::Common& vulkan, State& engine, DescriptorSet& desc_set,
    VkBuffer handle, int binding_idx, VkDeviceSize range )
{
    for ( size_t i = 0; i < engine.frame_overlap; ++i ) {
        VkDescriptorBufferInfo buffer_info = {
            .buffer = handle,
            .offset = 0,
            .range = range,
        };

        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = desc_set.handles[i],
            .dstBinding = static_cast<uint32_t>( binding_idx ),
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &buffer_info,
        };

        vkUpdateDescriptorSets( vulkan.device, 1, &write, 0, nullptr );
    }
}

void update_descriptor_set_image( vk::Common& vulkan, State& engine, DescriptorSet& desc_set,
    vk::mem::AllocatedImage img, int binding_idx )
{
    for ( size_t i = 0; i < engine.frame_overlap; ++i ) {
        VkDescriptorImageInfo desc_image_info = {
            .sampler = VK_NULL_HANDLE,
            .imageView = img.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        VkWriteDescriptorSet write_desc_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = desc_set.handles[i],
            .dstBinding = static_cast<uint32_t>( binding_idx ),
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo = &desc_image_info,
        };

        vkUpdateDescriptorSets( vulkan.device, 1, &write_desc_set, 0, nullptr );
    }
}

} // namespace racecar::engine
