#include "descriptors.hpp"

#include "../log.hpp"

namespace racecar::engine {

void create_descriptor_system(
    vk::Common& vulkan, uint32_t frame_overlap, DescriptorSystem& descriptor_system )
{
    std::vector<DescriptorAllocator::PoolSizeRatio> pool_sizes
        = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4 }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4 },
              { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4 }, { VK_DESCRIPTOR_TYPE_SAMPLER, 4 },
            { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 3 }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4 } };

    descriptor_allocator::init_pool( vulkan, descriptor_system.global_allocator, 200, pool_sizes );

    descriptor_system.frame_allocators = std::vector<DescriptorAllocator>( frame_overlap );

    for ( uint32_t i = 0; i < frame_overlap; i++ ) {
        descriptor_allocator::init_pool(
            vulkan, descriptor_system.frame_allocators[i], 200, pool_sizes );
    }

    log::info( "[engine] Created descriptor system" );
};

namespace descriptor_layout_builder {

void add_binding(
    DescriptorLayoutBuilder& ds_layout_builder, uint32_t binding, VkDescriptorType type )
{
    // Stage flags are not set here, will be later in descriptor_layout_builder::build
    VkDescriptorSetLayoutBinding new_binding = {
        .binding = binding,
        .descriptorType = type,
        .descriptorCount = 1,
    };

    ds_layout_builder.bindings.push_back( std::move( new_binding ) );
}

void clear( DescriptorLayoutBuilder& ds_layout_builder ) { ds_layout_builder.bindings.clear(); }

VkDescriptorSetLayout build( vk::Common& vulkan, VkShaderStageFlags shader_stage_flags,
    DescriptorLayoutBuilder& ds_layout_builder, VkDescriptorSetLayoutCreateFlags ds_layout_flags )
{
    for ( auto& binding : ds_layout_builder.bindings ) {
        binding.stageFlags |= shader_stage_flags;
    }

    VkDescriptorSetLayoutCreateInfo ds_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .flags = ds_layout_flags,
        .bindingCount = static_cast<uint32_t>( ds_layout_builder.bindings.size() ),
        .pBindings = ds_layout_builder.bindings.data(),
    };

    VkDescriptorSetLayout ds_layout = VK_NULL_HANDLE;
    vk::check(
        vkCreateDescriptorSetLayout( vulkan.device, &ds_layout_create_info, nullptr, &ds_layout ),
        "Failed to create descriptor set layout" );
    vulkan.destructor_stack.push( vulkan.device, ds_layout, vkDestroyDescriptorSetLayout );

    return ds_layout;
}

} // namespace descriptor_layout_builder

namespace descriptor_allocator {

void init_pool( vk::Common& vulkan, DescriptorAllocator& ds_allocator, uint32_t max_sets,
    std::span<DescriptorAllocator::PoolSizeRatio> pool_ratios )
{
    std::vector<VkDescriptorPoolSize> pool_sizes;
    for ( DescriptorAllocator::PoolSizeRatio ratio : pool_ratios ) {
        pool_sizes.push_back( VkDescriptorPoolSize {
            .type = ratio.type,
            .descriptorCount = static_cast<uint32_t>( ratio.ratio * max_sets ),
        } );
    }

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = 0,
        .maxSets = max_sets,
        .poolSizeCount = static_cast<uint32_t>( pool_sizes.size() ),
        .pPoolSizes = pool_sizes.data(),
    };

    vk::check( vkCreateDescriptorPool( vulkan.device, &pool_info, nullptr, &ds_allocator.pool ),
        "Failed to create descriptor pool" );
    vulkan.destructor_stack.push( vulkan.device, ds_allocator.pool, vkDestroyDescriptorPool );
}

void clear_descriptors( const vk::Common& vulkan, DescriptorAllocator& ds_allocator )
{
    vkResetDescriptorPool( vulkan.device, ds_allocator.pool, 0 );
}

VkDescriptorSet allocate(
    vk::Common& vulkan, const DescriptorAllocator& ds_allocator, VkDescriptorSetLayout layout )
{
    VkDescriptorSetAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = ds_allocator.pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    vk::check( vkAllocateDescriptorSets( vulkan.device, &allocate_info, &descriptor_set ),
        "Failed to allocate descriptor set" );
    /// TODO: destroy descriptor set
    // vulkan.destructor_stack.destructors.push( [=]() {
    //     VkDescriptorSet descriptor_set_copy = descriptor_set;
    //     vkFreeDescriptorSets( vulkan.device, ds_allocator.pool, 1, &descriptor_set_copy );
    // } );

    return descriptor_set;
}

} // namespace descriptor_allocator

void write_image( DescriptorWriter& writer, int binding, VkImageView image, VkSampler sampler,
    VkImageLayout layout, VkDescriptorType type )
{
    VkDescriptorImageInfo& info = writer.image_infos.emplace_back( VkDescriptorImageInfo {
        .sampler = sampler,
        .imageView = image,
        .imageLayout = layout,
    } );

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = VK_NULL_HANDLE,
        .dstBinding = static_cast<uint32_t>( binding ),
        .descriptorCount = 1,
        .descriptorType = type,
        .pImageInfo = &info,
    };

    writer.writes.push_back( write );
}

void write_buffer( DescriptorWriter& writer, int binding, VkBuffer buffer, size_t size,
    size_t offset, VkDescriptorType type )
{
    VkDescriptorBufferInfo& info = writer.buffer_infos.emplace_back(
        VkDescriptorBufferInfo { .buffer = buffer, .offset = offset, .range = size } );

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = static_cast<uint32_t>( binding ),
        .descriptorCount = 1,
        .descriptorType = type,
        .pBufferInfo = &info,
    };

    writer.writes.push_back( write );
}

void update_set( DescriptorWriter& writer, VkDevice device, VkDescriptorSet set )
{
    for ( VkWriteDescriptorSet& write : writer.writes ) {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(
        device, static_cast<uint32_t>( writer.writes.size() ), writer.writes.data(), 0, nullptr );
}

} // namespace racecar::engine
