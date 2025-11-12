#pragma once

#include "../vk/mem.hpp"
#include "descriptors.hpp"
#include "state.hpp"

#include <volk.h>

namespace racecar {

// I'm sorry yall
struct IUniformBuffer {
    virtual vk::mem::AllocatedBuffer buffer(size_t) = 0;
    virtual VkDescriptorSetLayout layout(size_t) = 0;
    virtual VkDescriptorSet* descriptor(size_t) = 0;
    virtual void update(racecar::vk::Common& vulkan, size_t f_idx) = 0;
};

template <typename T>
struct UniformBuffer : IUniformBuffer {
    bool dirty = false;
    std::vector<VkDescriptorSet> resource_descriptor;

    vk::mem::AllocatedBuffer buffer(size_t f_idx) override {
        return _buffer[f_idx];
    }

    VkDescriptorSetLayout layout(size_t f_idx) override {
        return _layout[f_idx];
    }
    
    VkDescriptorSet* descriptor(size_t f_idx) override {
        return &resource_descriptor[f_idx];
    }

    UniformBuffer(T d, std::vector<VkDescriptorSetLayout> l, std::vector<vk::mem::AllocatedBuffer> b, std::vector<VkDescriptorSet> r)
        : resource_descriptor(r), data(d), _layout(l), _buffer(b) {}

    void update([[maybe_unused]] racecar::vk::Common& vulkan, size_t f_idx) override {
        if (!dirty) {
            return;
        }

        memcpy( _buffer[f_idx].info.pMappedData, &data, sizeof(T) );

        engine::DescriptorWriter writer;
        write_buffer( writer, 0, buffer(f_idx).handle, sizeof(T), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        update_set( writer, vulkan.device, resource_descriptor[f_idx]);

        dirty = false;
    }

    T get_data() {
        return data;
    }

    void set_data(T t) {
        data = t;
        dirty = true;
    }

private:
    T data;
    std::vector<VkDescriptorSetLayout> _layout;
    std::vector<vk::mem::AllocatedBuffer> _buffer;
};

template <typename T>
UniformBuffer<T> create_uniform_buffer( racecar::vk::Common& vulkan, racecar::engine::State& engine, T input, VkShaderStageFlagBits shader_stages, int frame_overlap ) {
    auto data = input;

    std::vector<vk::mem::AllocatedBuffer> buffers((size_t)(frame_overlap));
    for(size_t i = 0; i < (size_t)frame_overlap; i++) {
        buffers[i] = vk::mem::create_buffer( vulkan, sizeof(T),
                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU ).value();
    }
        
    engine::DescriptorLayoutBuilder builder;
    engine::descriptor_layout_builder::add_binding( builder, 0,
                                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
    std::vector<VkDescriptorSetLayout> layout((size_t)frame_overlap);

    for (size_t i = 0; i < (size_t)frame_overlap; i++) {
        layout[i] = engine::descriptor_layout_builder::build(
            vulkan, shader_stages, builder );
    }
            
    std::vector<VkDescriptorSet> r_descriptor((size_t)frame_overlap);
    for (size_t i = 0; i < (size_t)frame_overlap; i++) {
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