#pragma once

#include "../vk/common.hpp"
#include "uniform_buffers.hpp"

#include <deque>
#include <span>

namespace racecar::engine {

struct DescriptorAllocator {
    struct PoolSizeRatio {
        VkDescriptorType type;
        float ratio;
    };

    VkDescriptorPool pool;
};

struct DescriptorLayoutBuilder {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};

struct DescriptorSystem {
    // Global allocator should be used for static resources (images, samplers, views, etc.)
    // Frame allocator should be used for changing frame-dependent resources (camera changing
    // buffers with time, etc.)
    DescriptorAllocator global_allocator = {};
    std::vector<DescriptorAllocator> frame_allocators;

    // This is going to be a pain to document, but essentially ANY custom layout we define MUST be
    // done in here. I don't have a better solution at the moment, so bear with me and deal with it.
    // Thanks. I'm very open to suggestions!
    VkDescriptorSetLayout triangle_test_layout = nullptr;

    // This pointer gets populated during create_descriptor_system.
    // VkDescriptorSetLayout camera_set_layout = nullptr;

    // It's intended that this camera uniform information gets made per frame.
    // For now, for simplicity, let's ignore.
    // uniform_buffer::CameraBufferData camera_data;

    // std::vector<vk::mem::UniformBuffer> camera_buffers;
};

struct LayoutResource {
    VkDescriptorSetLayout layout;
    size_t data_size;
    void* source_data;
};

bool create_descriptor_system( const vk::Common& vulkan,
                               uint32_t frame_overlap,
                               DescriptorSystem& descriptor_system );

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

void add_binding( DescriptorLayoutBuilder& ds_layout_builder,
                  uint32_t binding,
                  VkDescriptorType type );

void clear( DescriptorLayoutBuilder& ds_layout_builder );

VkDescriptorSetLayout build( const vk::Common& vulkan,
                             VkShaderStageFlags shader_stages,
                             DescriptorLayoutBuilder& ds_layout_builder,
                             VkDescriptorSetLayoutCreateFlags flags = 0 );

}  // namespace descriptor_layout_builder

// For actual memory allocation for our descriptors, we build a descriptor allocator.
// To my understanding, a descriptor pool manages descriptor sets, and it's where they're allocated.
namespace descriptor_allocator {

bool init_pool( const vk::Common& vulkan,
                DescriptorAllocator& ds_allocator,
                uint32_t max_sets,
                std::span<DescriptorAllocator::PoolSizeRatio> poolRatios );

void clear_descriptors( const vk::Common& vulkan, DescriptorAllocator& ds_allocator );

void destroy_pool( const vk::Common& vulkan, DescriptorAllocator& ds_allocator );

VkDescriptorSet allocate( const vk::Common& vulkan,
                          const DescriptorAllocator& ds_allocator,
                          VkDescriptorSetLayout layout );

}  // namespace descriptor_allocator

// Good lord the code gets more and more ugly.
struct DescriptorWriter {
    std::deque<VkDescriptorImageInfo> imageInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;
};

void write_image( DescriptorWriter descriptor_writer,
                  int binding,
                  VkImageView image,
                  VkSampler sampler,
                  VkImageLayout layout,
                  VkDescriptorType type );
                  
void write_buffer( DescriptorWriter& descriptor_writer,
                   int binding,
                   VkBuffer buffer,
                   size_t size,
                   size_t offset,
                   VkDescriptorType type );

void clear();
void update_set( DescriptorWriter& writer, VkDevice device, VkDescriptorSet set );

}  // namespace racecar::engine
