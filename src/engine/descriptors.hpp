#pragma once

#include "../vk/common.hpp"
#include "uniform_buffers.hpp"

#include <deque>
#include <span>

namespace racecar::engine {

struct DescriptorAllocator {
    struct PoolSizeRatio {
        VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
        float ratio = 0.f;
    };

    VkDescriptorPool pool = VK_NULL_HANDLE;
};

struct DescriptorLayoutBuilder {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};

struct DescriptorSystem {
    // Global allocator should be used for static resources (images, samplers, views, etc.)
    // Frame allocator should be used for changing frame-dependent resources (camera changing
    // buffers with time, etc.)
    DescriptorAllocator global_allocator;
    std::vector<DescriptorAllocator> frame_allocators;
};

struct LayoutResource {
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    size_t data_size;
    void* source_data;
    void* image_data = nullptr;
};

bool create_descriptor_system(
    const vk::Common& vulkan, uint32_t frame_overlap, DescriptorSystem& descriptor_system );

// Descriptors are essentially shader resources. This can be anything from a buffer, buffer view,
// image view, sampler, etc. For that general purpose reason, it makes sense to me that descriptors
// belong under engine, since we're dealing with GPU engine resources.
//
// Descriptors must there be defined via a descriptor layout, which is binded to the pipeline,
// describing what shader resources the pipeline can access. This is conceptually signature to a
// root signature in DX12 and the bindGroupLayout in WebGPU.
//
// This therefore motivates the creation of a DescriptorLayoutBuilder to build layouts with as many
// bindings as we need.
namespace descriptor_layout_builder {

void add_binding(
    DescriptorLayoutBuilder& ds_layout_builder, uint32_t binding, VkDescriptorType type );

void clear( DescriptorLayoutBuilder& ds_layout_builder );

VkDescriptorSetLayout build( const vk::Common& vulkan, VkShaderStageFlags shader_stages,
    DescriptorLayoutBuilder& ds_layout_builder, VkDescriptorSetLayoutCreateFlags flags = 0 );

} // namespace descriptor_layout_builder

// For actual memory allocation for our descriptors, we build a descriptor allocator.
// To my understanding, a descriptor pool manages descriptor sets, and it's where they're allocated.
namespace descriptor_allocator {

bool init_pool( const vk::Common& vulkan, DescriptorAllocator& ds_allocator, uint32_t max_sets,
    std::span<DescriptorAllocator::PoolSizeRatio> poolRatios );

void clear_descriptors( const vk::Common& vulkan, DescriptorAllocator& ds_allocator );

void destroy_pool( const vk::Common& vulkan, DescriptorAllocator& ds_allocator );

VkDescriptorSet allocate( const vk::Common& vulkan, const DescriptorAllocator& ds_allocator,
    VkDescriptorSetLayout layout );

} // namespace descriptor_allocator

struct DescriptorWriter {
    std::deque<VkDescriptorImageInfo> image_infos;
    std::deque<VkDescriptorBufferInfo> buffer_infos;
    std::vector<VkWriteDescriptorSet> writes;
};

void write_image( DescriptorWriter& writer, int binding, VkImageView image, VkSampler sampler,
    VkImageLayout layout, VkDescriptorType type );

void write_buffer( DescriptorWriter& descriptor_writer, int binding, VkBuffer buffer, size_t size,
    size_t offset, VkDescriptorType type );

void clear();
void update_set( DescriptorWriter& writer, VkDevice device, VkDescriptorSet set );

} // namespace racecar::engine
