#pragma once

#include "../log.hpp"
#include "../vk/mem.hpp"

#include <volk.h>

#include <algorithm>
#include <vector>

namespace racecar {

struct IUniformBuffer {
    virtual vk::mem::AllocatedBuffer buffer( size_t ) const = 0;
    virtual void update( size_t frame_idx ) = 0;
    virtual void update_all() = 0;

    virtual ~IUniformBuffer() { }
};

template <typename T> struct UniformBuffer : IUniformBuffer {
    UniformBuffer() = default;

    UniformBuffer( T data, std::vector<vk::mem::AllocatedBuffer> buffer )
        : data_( data )
        , buffer_( buffer )
        , dirty_( buffer.size(), false )
    {
    }

    vk::mem::AllocatedBuffer buffer( size_t frame_idx ) const override
    {
        return buffer_[frame_idx];
    }

    // Update current frame's buffer (Runtime)
    void update( size_t frame_idx ) override
    {
        const vk::Common& vulkan = vk::Common::GetConst();

        if ( frame_idx >= dirty_.size() || !dirty_[frame_idx] ) {
            return;
        }

        std::memcpy( buffer_[frame_idx].info.pMappedData, &data_, sizeof( T ) );
        vmaFlushAllocation( vulkan.allocator, buffer_[frame_idx].allocation, 0, sizeof( T ) );

        dirty_[frame_idx] = false;
    }

    // Update all frame's buffers (Setup)
    void update_all() override
    {
        for ( size_t i = 0; i < buffer_.size(); ++i ) {
            update( i );
        }
    }

    T get_data() const { return data_; }

    void set_data( T t )
    {
        data_ = t;
        std::fill( dirty_.begin(), dirty_.end(), true );
    }

private:
    T data_ = {};
    std::vector<VkDescriptorSetLayout> layout_;
    std::vector<vk::mem::AllocatedBuffer> buffer_;
    std::vector<bool> dirty_;
};

template <typename T>
UniformBuffer<T> create_uniform_buffer( T input, size_t frame_overlap )
{
    std::vector<vk::mem::AllocatedBuffer> buffers( frame_overlap );

    for ( size_t i = 0; i < frame_overlap; ++i ) {
        try {
            buffers[i] = vk::mem::create_buffer( sizeof( T ),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU );
        } catch ( const Exception& ex ) {
            log::error( "Failed to create uniform buffer {} for swapchain", i );
            throw;
        }
    }

    return UniformBuffer( input, buffers );
}

} // namespace racecar
