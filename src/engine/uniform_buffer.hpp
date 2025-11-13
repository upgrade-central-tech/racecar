#pragma once

#include "../log.hpp"
#include "../vk/mem.hpp"

#include <volk.h>

namespace racecar {

struct IUniformBuffer {
    virtual vk::mem::AllocatedBuffer buffer( size_t ) = 0;
    virtual void update( racecar::vk::Common& vulkan, size_t frame_idx ) = 0;

    virtual ~IUniformBuffer() { }
};

template <typename T> struct UniformBuffer : IUniformBuffer {
    bool dirty = false;

    UniformBuffer( T data, std::vector<vk::mem::AllocatedBuffer> buffer )
        : data_( data )
        , buffer_( buffer )
    {
    }

    vk::mem::AllocatedBuffer buffer( size_t frame_idx ) override { return buffer_[frame_idx]; }

    void update( racecar::vk::Common& vulkan, size_t frame_idx ) override
    {
        if ( !dirty ) {
            return;
        }

        std::memcpy( buffer_[frame_idx].info.pMappedData, &data_, sizeof( T ) );
        vmaFlushAllocation( vulkan.allocator, buffer_[frame_idx].allocation, 0, sizeof( T ) );

        dirty = false;
    }

    T get_data() { return data_; }

    void set_data( T t )
    {
        data_ = t;
        dirty = true;
    }

private:
    T data_;
    std::vector<VkDescriptorSetLayout> layout_;
    std::vector<vk::mem::AllocatedBuffer> buffer_;
};

template <typename T>
UniformBuffer<T> create_uniform_buffer( racecar::vk::Common& vulkan, T input, size_t frame_overlap )
{
    std::vector<vk::mem::AllocatedBuffer> buffers( frame_overlap );

    for ( size_t i = 0; i < frame_overlap; ++i ) {
        try {
            buffers[i] = vk::mem::create_buffer( vulkan, sizeof( T ),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU );
        } catch ( const Exception& ex ) {
            log::error( "Failed to create uniform buffer {} for swapchain", i );
            throw;
        }
    }

    return { input, buffers };
}

} // namespace racecar
