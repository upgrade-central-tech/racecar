#if RACECAR_RAY_TRACING

#include "reflection_mip_chain.hpp"

#include "../engine/pipeline_barrier.hpp"
#include "../vk/common.hpp"
#include "../vk/create.hpp"

#include <algorithm>

namespace racecar {

namespace {

constexpr std::string_view REFLECTION_MIPS_SHADER_MODULE_PATH
    = "../shaders/reflections/reflection_mips.spv";

constexpr std::string_view REFLECTION_UPSAMPLE_SHADER_MODULE_PATH
    = "../shaders/reflections/reflection_upsample.spv";

VkExtent2D mip_extent( VkExtent2D base, uint32_t mip )
{
    return VkExtent2D {
        .width = std::max( base.width >> mip, 1u ),
        .height = std::max( base.height >> mip, 1u ),
    };
}

VkImageSubresourceRange mip_range( uint32_t base_mip, uint32_t level_count )
{
    return VkImageSubresourceRange {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = base_mip,
        .levelCount = level_count,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
}

engine::ImageBarrier color_write_to_compute_read( engine::RWImage* image )
{
    return engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  .src_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                  .src_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                  .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                  .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                                  .dst_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  .image = image,
                                  .range = mip_range( 0, 1 ) };
}

engine::ImageBarrier undefined_to_compute_write( engine::RWImage* image )
{
    return engine::ImageBarrier {
        .src_stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        .src_access = VK_ACCESS_2_NONE,
        .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dst_access = VK_ACCESS_2_SHADER_WRITE_BIT,
        .dst_layout = VK_IMAGE_LAYOUT_GENERAL,
        .image = image,
        .range = mip_range( 1, ReflectionMipChain::DOWNSAMPLE_COUNT ),
    };
}

engine::ImageBarrier compute_read_to_compute_write( engine::RWImage* image, uint32_t mip )
{
    return engine::ImageBarrier {
        .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .src_access = VK_ACCESS_2_SHADER_READ_BIT,
        .src_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dst_access = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
        .dst_layout = VK_IMAGE_LAYOUT_GENERAL,
        .image = image,
        .range = mip_range( mip, 1 ),
    };
}

engine::ImageBarrier compute_write_to_compute_read( engine::RWImage* image, uint32_t mip )
{
    return engine::ImageBarrier {
        .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .src_access = VK_ACCESS_2_SHADER_WRITE_BIT,
        .src_layout = VK_IMAGE_LAYOUT_GENERAL,
        .dst_stage
        = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
        .dst_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image = image,
        .range = mip_range( mip, 1 ),
    };
}

}

void create_reflection_mip_chain_resources(
    ReflectionMipChain* chain_out,
    engine::RWImage& reflection_color,
    engine::RWImage& reflection_data
)
{
    ReflectionMipChain& chain = *chain_out;

    chain.reflection_color = &reflection_color;
    chain.reflection_data = &reflection_data;

    for ( uint32_t i = 0; i < ReflectionMipChain::DOWNSAMPLE_COUNT; ++i ) {
        engine::DescriptorSet& desc_set = chain.downsample_desc_sets[i];

        desc_set = engine::generate_descriptor_set(
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
              VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
              VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
            VK_SHADER_STAGE_COMPUTE_BIT
        );

        engine::update_descriptor_set_rwimage_mip(
            desc_set,
            reflection_color,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            0,
            i
        );
        engine::update_descriptor_set_rwimage_mip(
            desc_set,
            reflection_data,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            1,
            i
        );
        engine::update_descriptor_set_rwimage_mip(
            desc_set,
            reflection_color,
            VK_IMAGE_LAYOUT_GENERAL,
            2,
            i + 1
        );
        engine::update_descriptor_set_rwimage_mip(
            desc_set,
            reflection_data,
            VK_IMAGE_LAYOUT_GENERAL,
            3,
            i + 1
        );
    }

    chain.downsample_pipeline = engine::create_compute_pipeline(
        { chain.downsample_desc_sets[0].layouts[0] },
        vk::create::shader_module( REFLECTION_MIPS_SHADER_MODULE_PATH ),
        "cs_reflection_mips"
    );

    const vk::Common& vulkan = vk::Common::GetConst();

    chain.sampler_desc_set = engine::generate_descriptor_set(
        { VK_DESCRIPTOR_TYPE_SAMPLER },
        VK_SHADER_STAGE_COMPUTE_BIT
    );
    engine::update_descriptor_set_sampler(
        chain.sampler_desc_set,
        vulkan.global_samplers.linear_sampler,
        0
    );

    for ( uint32_t i = 0; i < ReflectionMipChain::UPSAMPLE_COUNT; ++i ) {
        const uint32_t dst_mip = ReflectionMipChain::UPSAMPLE_COUNT - i;
        engine::DescriptorSet& desc_set = chain.upsample_desc_sets[i];

        desc_set = engine::generate_descriptor_set(
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
              VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
              VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
              VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
            VK_SHADER_STAGE_COMPUTE_BIT
        );

        engine::update_descriptor_set_rwimage_mip(
            desc_set,
            reflection_color,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            0,
            dst_mip + 1
        );
        engine::update_descriptor_set_rwimage_mip(
            desc_set,
            reflection_data,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            1,
            dst_mip + 1
        );
        engine::update_descriptor_set_rwimage_mip(
            desc_set,
            reflection_data,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            2,
            dst_mip
        );
        engine::update_descriptor_set_rwimage_mip(
            desc_set,
            reflection_color,
            VK_IMAGE_LAYOUT_GENERAL,
            3,
            dst_mip
        );
    }

    chain.upsample_pipeline = engine::create_compute_pipeline(
        { chain.upsample_desc_sets[0].layouts[0], chain.sampler_desc_set.layouts[0] },
        vk::create::shader_module( REFLECTION_UPSAMPLE_SHADER_MODULE_PATH ),
        "cs_reflection_upsample"
    );
}

void add_reflection_mip_chain_pass( ReflectionMipChain& chain, engine::TaskList& task_list )
{
    const engine::State& engine = engine::State::GetConst();

    engine::add_pipeline_barrier(
        task_list,
        engine::PipelineBarrierDescriptor {
            .buffer_barriers = { },
            .image_barriers = {
                color_write_to_compute_read( chain.reflection_color ),
                color_write_to_compute_read( chain.reflection_data ),
                undefined_to_compute_write( chain.reflection_color ),
                undefined_to_compute_write( chain.reflection_data ),
            } }
    );

    for ( uint32_t i = 0; i < ReflectionMipChain::DOWNSAMPLE_COUNT; ++i ) {
        const uint32_t dst_mip = i + 1;
        const VkExtent2D extent = mip_extent( engine.swapchain.extent, dst_mip );

        engine::add_cs_task(
            task_list,
            engine::ComputeTask {
                .pipeline = chain.downsample_pipeline,
                .descriptor_sets = { &chain.downsample_desc_sets[i] },
                .group_size = glm::ivec3(
                    static_cast<int32_t>( ( extent.width + 7 ) / 8 ),
                    static_cast<int32_t>( ( extent.height + 7 ) / 8 ),
                    1
                ),
            }
        );

        engine::add_pipeline_barrier(
            task_list,
            engine::PipelineBarrierDescriptor {
                .buffer_barriers = { },
                .image_barriers = {
                    compute_write_to_compute_read( chain.reflection_color, dst_mip ),
                    compute_write_to_compute_read( chain.reflection_data, dst_mip ),
                } }
        );
    }

    for ( uint32_t i = 0; i < ReflectionMipChain::UPSAMPLE_COUNT; ++i ) {
        const uint32_t dst_mip = ReflectionMipChain::UPSAMPLE_COUNT - i;
        const VkExtent2D extent = mip_extent( engine.swapchain.extent, dst_mip );

        engine::add_pipeline_barrier(
            task_list,
            engine::PipelineBarrierDescriptor {
                .buffer_barriers = { },
                .image_barriers
                = { compute_read_to_compute_write( chain.reflection_color, dst_mip ) } }
        );

        engine::add_cs_task(
            task_list,
            engine::ComputeTask {
                .pipeline = chain.upsample_pipeline,
                .descriptor_sets = { &chain.upsample_desc_sets[i], &chain.sampler_desc_set },
                .group_size = glm::ivec3(
                    static_cast<int32_t>( ( extent.width + 7 ) / 8 ),
                    static_cast<int32_t>( ( extent.height + 7 ) / 8 ),
                    1
                ),
            }
        );

        engine::add_pipeline_barrier(
            task_list,
            engine::PipelineBarrierDescriptor {
                .buffer_barriers = { },
                .image_barriers
                = { compute_write_to_compute_read( chain.reflection_color, dst_mip ) } }
        );
    }
}

}

#endif // RACECAR_RAY_TRACING
