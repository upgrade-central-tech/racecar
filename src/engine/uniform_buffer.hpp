#pragma once

#include "../vk/mem.hpp"
#include "descriptors.hpp"
#include "state.hpp"

#include <volk.h>

namespace racecar {

// I'm sorry yall
struct IUniformBuffer {
    virtual vk::mem::AllocatedBuffer buffer(int) = 0;
    virtual VkDescriptorSetLayout layout(int) = 0;
    virtual VkDescriptorSet* descriptor(int) = 0;
};

template <typename T>
struct UniformBuffer : IUniformBuffer {
    T data;
    std::vector<VkDescriptorSet> resource_descriptor;

    vk::mem::AllocatedBuffer buffer(int f_idx) override {
        return _buffer[f_idx];
    }

    VkDescriptorSetLayout layout(int f_idx) override {
        return _layout[f_idx];
    }
    
    VkDescriptorSet* descriptor(int f_idx) override {
        return &resource_descriptor[f_idx];
    }

    UniformBuffer(T d, std::vector<VkDescriptorSetLayout> l, std::vector<vk::mem::AllocatedBuffer> b, std::vector<VkDescriptorSet> r)
        : data(d), resource_descriptor(r), _layout(l), _buffer(b) {}

    void update([[maybe_unused]] racecar::vk::Common& vulkan, int f_idx) {
        memcpy( _buffer[f_idx].info.pMappedData, &data, sizeof(T) );

        engine::DescriptorWriter writer;
        write_buffer( writer, 0, buffer(f_idx).handle, sizeof(T), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        update_set( writer, vulkan.device, resource_descriptor[f_idx]);
    }

private:
    std::vector<VkDescriptorSetLayout> _layout;
    std::vector<vk::mem::AllocatedBuffer> _buffer;
};

template <typename T>
UniformBuffer<T> create_uniform_buffer( racecar::vk::Common& vulkan, racecar::engine::State& engine, T input, VkShaderStageFlagBits shader_stages, int frame_overlap ) {
    auto data = input;

    
    std::vector<vk::mem::AllocatedBuffer> buffers(frame_overlap);
    for(int i = 0; i < frame_overlap; i++) {
        buffers[i] = vk::mem::create_buffer( vulkan, sizeof(T),
                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU ).value();
    }
        
    engine::DescriptorLayoutBuilder builder;
    engine::descriptor_layout_builder::add_binding( builder, 0,
                                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
    std::vector<VkDescriptorSetLayout> layout(frame_overlap);

    for (int i = 0; i < frame_overlap; i++) {
        layout[i] = engine::descriptor_layout_builder::build(
            vulkan, shader_stages, builder );
    }
            
    std::vector<VkDescriptorSet> r_descriptor(frame_overlap);
    for (int i = 0; i < frame_overlap; i++) {
        r_descriptor[i] = engine::descriptor_allocator::allocate(
            vulkan, engine.descriptor_system.frame_allocators[i], layout[i] );
    }

    SDL_Log("WRITE");
    UniformBuffer<T> u_buffer (
        data,
        layout,
        buffers,
        r_descriptor
    );

    return u_buffer;
}

}