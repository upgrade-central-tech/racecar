#include "descriptor_set.hpp"

#include "descriptors.hpp"
#include "state.hpp"

namespace racecar::engine {

DescriptorSet generate_descriptor_set(
    const std::vector<VkDescriptorType>& types, VkShaderStageFlags shader_stage_flags
)
{
    engine::State& engine = engine::State::GetMut();
    const size_t num_frames = engine.swapchain_images.size();

    DescriptorSet desc_set = {
        .descriptor_sets = std::vector<VkDescriptorSet>( num_frames ),
        .layouts = std::vector<VkDescriptorSetLayout>( num_frames ),
    };

    try {
        engine::DescriptorLayoutBuilder builder;

        for ( uint32_t i = 0; i < static_cast<uint32_t>( types.size() ); ++i ) {
            engine::descriptor_layout_builder::add_binding( builder, i, types[i] );
        }

        for ( size_t i = 0; i < num_frames; ++i ) {
            desc_set.layouts[i]
                = engine::descriptor_layout_builder::build( shader_stage_flags, builder );
            desc_set.descriptor_sets[i] = engine::descriptor_allocator::allocate(
                engine.descriptor_system.frame_allocators[i],
                desc_set.layouts[i]
            );
        }
    } catch ( const Exception& ex ) {
        log::error( "[DescriptorSet] Failed to generate" );
        throw;
    }

    return desc_set;
}

DescriptorSet generate_array_descriptor_set(
    const std::vector<VkDescriptorType>& types,
    VkShaderStageFlags shader_stage_flags,
    uint32_t count
)
{
    engine::State& engine = engine::State::GetMut();
    const size_t num_frames = engine.swapchain_images.size();

    DescriptorSet desc_set = {
        .descriptor_sets = std::vector<VkDescriptorSet>( num_frames ),
        .layouts = std::vector<VkDescriptorSetLayout>( num_frames ),
    };

    try {
        engine::DescriptorLayoutBuilder builder;

        for ( uint32_t i = 0; i < static_cast<uint32_t>( types.size() ); ++i ) {
            engine::descriptor_layout_builder::add_array_binding( builder, i, types[i], count );
        }

        for ( size_t i = 0; i < num_frames; ++i ) {
            desc_set.layouts[i]
                = engine::descriptor_layout_builder::build( shader_stage_flags, builder );
            desc_set.descriptor_sets[i] = engine::descriptor_allocator::allocate(
                engine.descriptor_system.frame_allocators[i],
                desc_set.layouts[i]
            );
        }
    } catch ( const Exception& ex ) {
        log::error( "[DescriptorSet] Failed to generate" );
        throw;
    }

    return desc_set;
}

void update_descriptor_set_image(
    DescriptorSet& desc_set, const vk::mem::AllocatedImage& img, int binding_idx
)
{
    const vk::Common& vulkan = vk::Common::GetConst();
    const engine::State& engine = engine::State::GetConst();
    for ( size_t i = 0; i < engine.frame_overlap; ++i ) {
        VkDescriptorImageInfo desc_image_info = {
            .sampler = VK_NULL_HANDLE,
            .imageView = img.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        VkWriteDescriptorSet write_desc_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = desc_set.descriptor_sets[i],
            .dstBinding = static_cast<uint32_t>( binding_idx ),
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo = &desc_image_info,
        };

        vkUpdateDescriptorSets( vulkan.device, 1, &write_desc_set, 0, nullptr );
    }
}

void update_descriptor_set_image_array(
    DescriptorSet& desc_set, const std::vector<vk::mem::AllocatedImage>& imgs, int binding_idx
)
{
    const vk::Common& vulkan = vk::Common::GetConst();
    const engine::State& engine = engine::State::GetConst();
    if ( imgs.empty() ) {
        return;
    }

    for ( size_t i = 0; i < engine.frame_overlap; ++i ) {
        std::vector<VkDescriptorImageInfo> image_infos;
        for ( size_t img = 0; img < imgs.size(); img++ ) {
            image_infos.push_back(
                {
                    .sampler = VK_NULL_HANDLE,
                    .imageView = imgs[img].image_view,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                }
            );
        }
        VkWriteDescriptorSet write_desc_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = desc_set.descriptor_sets[i],
            .dstBinding = static_cast<uint32_t>( binding_idx ),
            .descriptorCount = uint32_t( imgs.size() ),
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo = image_infos.data(),
        };

        vkUpdateDescriptorSets( vulkan.device, 1, &write_desc_set, 0, nullptr );
    }
}

void update_descriptor_set_rwimage_array(
    DescriptorSet& desc_set,
    const std::vector<const RWImage*>& imgs,
    VkImageLayout img_layout,
    int binding_idx,
    uint32_t array_count
)
{
    const vk::Common& vulkan = vk::Common::GetConst();
    const engine::State& engine = engine::State::GetConst();
    if ( imgs.empty() || array_count == 0 ) {
        return;
    }

    for ( size_t i = 0; i < engine.frame_overlap; ++i ) {
        std::vector<VkDescriptorImageInfo> image_infos;

        for ( uint32_t slot = 0; slot < array_count; slot++ ) {
            // Repeat the last entry across the unused tail. The shader never reads those slots,
            // but leaving descriptors unwritten is invalid.
            size_t img = std::min( size_t( slot ), imgs.size() - 1 );

            image_infos.push_back(
                {
                    .sampler = VK_NULL_HANDLE,
                    .imageView = imgs[img]->images[i].image_view,
                    .imageLayout = img_layout,
                }
            );
        }

        VkWriteDescriptorSet write_desc_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = desc_set.descriptor_sets[i],
            .dstBinding = static_cast<uint32_t>( binding_idx ),
            .descriptorCount = array_count,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo = image_infos.data(),
        };

        vkUpdateDescriptorSets( vulkan.device, 1, &write_desc_set, 0, nullptr );
    }
}

void update_descriptor_set_rwimage(
    DescriptorSet& desc_set, const RWImage& rw_img, VkImageLayout img_layout, int binding_idx
)
{
    const vk::Common& vulkan = vk::Common::GetConst();
    const engine::State& engine = engine::State::GetConst();
    for ( size_t i = 0; i < engine.frame_overlap; ++i ) {
        const vk::mem::AllocatedImage& alloc_image = rw_img.images[i];

        VkDescriptorImageInfo desc_image_info = {
            .sampler = VK_NULL_HANDLE,
            .imageView = alloc_image.image_view,
            .imageLayout = img_layout, // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        VkWriteDescriptorSet write_desc_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = desc_set.descriptor_sets[i],
            .dstBinding = static_cast<uint32_t>( binding_idx ),
            .descriptorCount = 1,
            .descriptorType = ( img_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL )
                ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
                : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &desc_image_info,
        };

        vkUpdateDescriptorSets( vulkan.device, 1, &write_desc_set, 0, nullptr );
    }
}

void update_descriptor_set_rwimage_mip(
    DescriptorSet& desc_set,
    const RWImage& rw_img,
    VkImageLayout img_layout,
    int binding_idx,
    size_t mip
)
{
    const vk::Common& vulkan = vk::Common::GetConst();
    const engine::State& engine = engine::State::GetConst();
    for ( size_t i = 0; i < engine.frame_overlap; ++i ) {
        const vk::mem::AllocatedImage& alloc_image = rw_img.images[i];

        VkDescriptorImageInfo desc_image_info = {
            .sampler = VK_NULL_HANDLE,
            .imageView = alloc_image.mip_levels[mip],
            .imageLayout = img_layout, // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        VkWriteDescriptorSet write_desc_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = desc_set.descriptor_sets[i],
            .dstBinding = static_cast<uint32_t>( binding_idx ),
            .descriptorCount = 1,
            .descriptorType = ( img_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL )
                ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
                : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &desc_image_info,
        };

        vkUpdateDescriptorSets( vulkan.device, 1, &write_desc_set, 0, nullptr );
    }
}

void update_descriptor_set_depth_image(
    DescriptorSet& desc_set, const RWImage& depth_img, int binding_idx
)
{
    const vk::Common& vulkan = vk::Common::GetConst();
    const engine::State& engine = engine::State::GetConst();
    for ( size_t i = 0; i < engine.frame_overlap; ++i ) {
        const vk::mem::AllocatedImage& img = depth_img.images[i];
        VkDescriptorImageInfo desc_image_info = {
            .sampler = VK_NULL_HANDLE,
            .imageView = img.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
        };

        VkWriteDescriptorSet write_desc_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = desc_set.descriptor_sets[i],
            .dstBinding = static_cast<uint32_t>( binding_idx ),
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo = &desc_image_info,
        };

        vkUpdateDescriptorSets( vulkan.device, 1, &write_desc_set, 0, nullptr );
    }
}

void update_descriptor_set_write_image(
    DescriptorSet& desc_set, const vk::mem::AllocatedImage& img, int binding_idx
)
{
    const vk::Common& vulkan = vk::Common::GetConst();
    const engine::State& engine = engine::State::GetConst();
    for ( size_t i = 0; i < engine.frame_overlap; ++i ) {
        VkDescriptorImageInfo desc_image_info = {
            .sampler = VK_NULL_HANDLE,
            .imageView = img.storage_image_view,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };

        VkWriteDescriptorSet write_desc_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = desc_set.descriptor_sets[i],
            .dstBinding = static_cast<uint32_t>( binding_idx ),
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &desc_image_info,
        };

        vkUpdateDescriptorSets( vulkan.device, 1, &write_desc_set, 0, nullptr );
    }
}

void update_descriptor_set_sampler( DescriptorSet& desc_set, VkSampler sampler, int binding_idx )
{
    const vk::Common& vulkan = vk::Common::GetConst();
    const engine::State& engine = engine::State::GetConst();
    for ( size_t i = 0; i < engine.frame_overlap; ++i ) {
        VkDescriptorImageInfo desc_image_info = {
            .sampler = sampler,
            .imageView = VK_NULL_HANDLE,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        VkWriteDescriptorSet write_desc_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = desc_set.descriptor_sets[i],
            .dstBinding = static_cast<uint32_t>( binding_idx ),
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .pImageInfo = &desc_image_info,
        };

        vkUpdateDescriptorSets( vulkan.device, 1, &write_desc_set, 0, nullptr );
    }
}

#if RACECAR_RAY_TRACING
void update_descriptor_set_acceleration_structure(
    DescriptorSet& desc_set, VkAccelerationStructureKHR tlas, int binding_idx
)
{
    const vk::Common& vulkan = vk::Common::GetConst();
    const engine::State& engine = engine::State::GetConst();
    for ( size_t i = 0; i < engine.frame_overlap; ++i ) {
        VkWriteDescriptorSetAccelerationStructureKHR desc_as_info
            = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                .pNext = VK_NULL_HANDLE,
                .accelerationStructureCount = 1,
                .pAccelerationStructures = &tlas };

        VkWriteDescriptorSet write_desc_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &desc_as_info, // IMPORTANT
            .dstSet = desc_set.descriptor_sets[i],
            .dstBinding = static_cast<uint32_t>( binding_idx ),
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        };

        vkUpdateDescriptorSets( vulkan.device, 1, &write_desc_set, 0, nullptr );
    }
}

void update_descriptor_set_acceleration_structure_per_frame(
    DescriptorSet& desc_set, const std::vector<VkAccelerationStructureKHR>& tlases, int binding_idx
)
{
    const vk::Common& vulkan = vk::Common::GetConst();
    const engine::State& engine = engine::State::GetConst();

    if ( tlases.size() != engine.frame_overlap ) {
        throw Exception(
            "[Update Descriptor Set Acceleration Structure Per Frame] Expected one "
            "acceleration structure per frame in flight"
        );
    }

    for ( size_t i = 0; i < engine.frame_overlap; ++i ) {
        VkWriteDescriptorSetAccelerationStructureKHR desc_as_info
            = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                .pNext = VK_NULL_HANDLE,
                .accelerationStructureCount = 1,
                .pAccelerationStructures = &tlases[i] };

        VkWriteDescriptorSet write_desc_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &desc_as_info,
            .dstSet = desc_set.descriptor_sets[i],
            .dstBinding = static_cast<uint32_t>( binding_idx ),
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        };

        vkUpdateDescriptorSets( vulkan.device, 1, &write_desc_set, 0, nullptr );
    }
}
#endif // RACECAR_RAY_TRACING

void update_descriptor_set_const_storage_buffer(
    DescriptorSet& desc_set, vk::mem::AllocatedBuffer storage_buffer, int binding_idx
)
{
    const vk::Common& vulkan = vk::Common::GetConst();
    const engine::State& engine = engine::State::GetConst();
    VkBuffer buffer = storage_buffer.handle;

    for ( size_t i = 0; i < engine.frame_overlap; ++i ) {
        VkDescriptorBufferInfo buffer_info = {
            .buffer = buffer,
            .offset = 0,
            .range = VK_WHOLE_SIZE,
        };

        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = desc_set.descriptor_sets[i],
            .dstBinding = static_cast<uint32_t>( binding_idx ),
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &buffer_info,
        };

        vkUpdateDescriptorSets( vulkan.device, 1, &write, 0, nullptr );
    }
}

// This is temporary and should eventually be replaced with a proper RWBuffer class
void update_descriptor_set_storage_buffer_per_frame(
    DescriptorSet& desc_set,
    const std::vector<vk::mem::AllocatedBuffer>& storage_buffers,
    int binding_idx
)
{
    const vk::Common& vulkan = vk::Common::GetConst();
    const engine::State& engine = engine::State::GetConst();

    if ( storage_buffers.size() != engine.frame_overlap ) {
        throw Exception(
            "[Update Descriptor Set Storage Buffer Per Frame] Expected one storage "
            "buffer per frame in flight"
        );
    }

    for ( size_t i = 0; i < engine.frame_overlap; ++i ) {
        VkDescriptorBufferInfo buffer_info = {
            .buffer = storage_buffers[i].handle,
            .offset = 0,
            .range = VK_WHOLE_SIZE,
        };

        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = desc_set.descriptor_sets[i],
            .dstBinding = static_cast<uint32_t>( binding_idx ),
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &buffer_info,
        };

        vkUpdateDescriptorSets( vulkan.device, 1, &write, 0, nullptr );
    }
}

} // namespace racecar::engine
