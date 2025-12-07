#include "bloom.hpp"

#include "../../log.hpp"
#include "../../vk/create.hpp"

namespace racecar::engine::post {

namespace {

constexpr std::string_view THRESHOLD_SHADER_PATH = "../shaders/post/bloom/threshold.spv";
constexpr std::string_view DOWNSAMPLE_SHADER_PATH = "../shaders/post/bloom/downsample.spv";
constexpr std::string_view UPSAMPLE_SHADER_PATH = "../shaders/post/bloom/upsample.spv";

}

BloomPass add_bloom( vk::Common& vulkan, const State& engine, TaskList& task_list, RWImage& inout,
    RWImage& write_only )
{
    BloomPass pass;

    engine::transition_cs_read_to_rw( task_list, inout );
    engine::transition_cs_write_to_rw( task_list, write_only );

    log::info( "[Post] [Bloom] Number of passes: {}", BloomPass::NUM_PASSES );

    {
        VkExtent3D current_extent
            = { engine.swapchain.extent.width / 2, engine.swapchain.extent.height / 2, 1 };

        for ( size_t i = 0; i < BloomPass::NUM_PASSES; ++i ) {
            pass.images[i] = engine::create_rwimage( vulkan, engine, current_extent,
                VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TYPE_2D, VK_SAMPLE_COUNT_1_BIT,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
                    | VK_IMAGE_USAGE_TRANSFER_SRC_BIT );

            engine::add_pipeline_barrier( task_list,
                { .image_barriers = { {
                      .src_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                      .src_access = VK_ACCESS_2_NONE,
                      .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                      .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                      .dst_layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                      .image = pass.images[i],
                      .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
                  } } } );

            // Each progressive image has half resolution
            log::info( "[Post] [Bloom] Created intermediate image of size {}×{}",
                current_extent.width, current_extent.height );
            current_extent = { current_extent.width / 2, current_extent.height / 2, 1 };
        }
    }

    {
        pass.bloom_ub = create_uniform_buffer<ub_data::Bloom>( vulkan, {}, engine.frame_overlap );

        engine::DescriptorSet uniform_desc_set = engine::generate_descriptor_set(
            vulkan, engine, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, VK_SHADER_STAGE_COMPUTE_BIT );
        engine::update_descriptor_set_uniform( vulkan, engine, uniform_desc_set, pass.bloom_ub, 0 );

        pass.uniform_desc_set
            = std::make_unique<engine::DescriptorSet>( std::move( uniform_desc_set ) );
    }

    {
        engine::DescriptorSet sampler_desc_set = engine::generate_descriptor_set(
            vulkan, engine, { VK_DESCRIPTOR_TYPE_SAMPLER }, VK_SHADER_STAGE_COMPUTE_BIT );
        engine::update_descriptor_set_sampler(
            vulkan, engine, sampler_desc_set, vulkan.global_samplers.linear_sampler, 0 );

        pass.sampler_desc_set
            = std::make_unique<engine::DescriptorSet>( std::move( sampler_desc_set ) );
    }

    {
        engine::DescriptorSet threshold_desc_set = engine::generate_descriptor_set( vulkan, engine,
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
            VK_SHADER_STAGE_COMPUTE_BIT );
        engine::update_descriptor_set_rwimage( vulkan, engine, threshold_desc_set, inout,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0 );
        engine::update_descriptor_set_rwimage( vulkan, engine, threshold_desc_set, write_only,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1 );

        engine::Pipeline threshold_pipeline = engine::create_compute_pipeline( vulkan,
            { threshold_desc_set.layouts[0], pass.uniform_desc_set->layouts[0] },
            vk::create::shader_module( vulkan, THRESHOLD_SHADER_PATH ), "threshold" );

        pass.threshold_desc_set
            = std::make_unique<engine::DescriptorSet>( std::move( threshold_desc_set ) );

        engine::add_cs_task( task_list,
            {
                .pipeline = threshold_pipeline,
                .descriptor_sets = { pass.threshold_desc_set.get(), pass.uniform_desc_set.get() },
                .group_size = { ( engine.swapchain.extent.width + 7 ) / 8,
                    ( engine.swapchain.extent.height + 7 ) / 8, 1 },
            } );
    }

    VkShaderModule downsample_shader = vk::create::shader_module( vulkan, DOWNSAMPLE_SHADER_PATH );

    for ( size_t i = 0; i < BloomPass::NUM_PASSES; ++i ) {
        if ( i == 0 ) {
            // We'll be reading from the full resolution input texture
            engine::transition_cs_read_to_write( task_list, pass.images[0] );
        } else {
            engine::transition_cs_write_to_read( task_list, pass.images[i - 1] );
            engine::transition_cs_read_to_write( task_list, pass.images[i] );
        }

        const RWImage& input_image = i == 0 ? write_only : pass.images[i - 1];
        VkExtent3D output_extent = pass.images[i].images[0].image_extent;

        engine::DescriptorSet downsample_desc_set = engine::generate_descriptor_set( vulkan, engine,
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
            VK_SHADER_STAGE_COMPUTE_BIT );
        engine::update_descriptor_set_rwimage( vulkan, engine, downsample_desc_set, input_image,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0 );
        engine::update_descriptor_set_rwimage(
            vulkan, engine, downsample_desc_set, pass.images[i], VK_IMAGE_LAYOUT_GENERAL, 1 );

        engine::Pipeline downsample_pipeline = engine::create_compute_pipeline( vulkan,
            { downsample_desc_set.layouts[0], pass.uniform_desc_set->layouts[0],
                pass.sampler_desc_set->layouts[0] },
            downsample_shader, "downsample" );

        pass.downsample_desc_sets[i]
            = std::make_unique<engine::DescriptorSet>( std::move( downsample_desc_set ) );

        engine::add_cs_task( task_list,
            {
                .pipeline = downsample_pipeline,
                .descriptor_sets = { pass.downsample_desc_sets[i].get(),
                    pass.uniform_desc_set.get(), pass.sampler_desc_set.get() },
                .group_size
                = { ( output_extent.width + 7 ) / 8, ( output_extent.height + 7 ) / 8, 1 },
            } );

        if ( i == 0 ) {
            engine::transition_cs_rw_to_write( task_list, write_only );
        }
    }

    engine::transition_cs_write_to_rw( task_list, pass.images[BloomPass::NUM_PASSES - 1] );

    VkShaderModule upsample_shader = vk::create::shader_module( vulkan, UPSAMPLE_SHADER_PATH );

    for ( int signed_i = BloomPass::NUM_PASSES - 1; signed_i >= 0; --signed_i ) {
        size_t i = static_cast<size_t>( signed_i );

        if ( i == 0 ) {
            // We'll be writing to the full resolution output texture
            engine::transition_cs_rw_to_read( task_list, pass.images[0] );
        } else {
            engine::transition_cs_rw_to_read( task_list, pass.images[i] );
            engine::transition_cs_read_to_rw( task_list, pass.images[i - 1] );
        }

        const RWImage& output_image = i == 0 ? inout : pass.images[i - 1];
        VkExtent3D output_extent = output_image.images[0].image_extent;

        engine::DescriptorSet upsample_desc_set = engine::generate_descriptor_set( vulkan, engine,
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
            VK_SHADER_STAGE_COMPUTE_BIT );
        engine::update_descriptor_set_rwimage( vulkan, engine, upsample_desc_set, pass.images[i],
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0 );
        engine::update_descriptor_set_rwimage(
            vulkan, engine, upsample_desc_set, output_image, VK_IMAGE_LAYOUT_GENERAL, 1 );

        engine::Pipeline upsample_pipeline = engine::create_compute_pipeline( vulkan,
            { upsample_desc_set.layouts[0], pass.uniform_desc_set->layouts[0],
                pass.sampler_desc_set->layouts[0] },
            upsample_shader, "upsample" );

        size_t desc_set_idx = BloomPass::NUM_PASSES - 1 - i;
        pass.upsample_desc_sets[desc_set_idx]
            = std::make_unique<engine::DescriptorSet>( std::move( upsample_desc_set ) );

        engine::add_cs_task( task_list,
            {
                .pipeline = upsample_pipeline,
                .descriptor_sets = { pass.upsample_desc_sets[desc_set_idx].get(),
                    pass.uniform_desc_set.get(), pass.sampler_desc_set.get() },
                .group_size
                = { ( output_extent.width + 7 ) / 8, ( output_extent.height + 7 ) / 8, 1 },
            } );
    }

    engine::transition_cs_rw_to_read( task_list, inout );

    log::info( "[Post] Added bloom pass!" );

    return pass;
}

}
