#include "bloom.hpp"

#include "../../log.hpp"
#include "../../vk/create.hpp"

namespace racecar::engine::post {

namespace {

constexpr std::string_view BRIGHTNESS_THRESHOLD_PATH
    = "../shaders/post/bloom/brightness_threshold.spv";
constexpr std::string_view HORZ_BLUR_PATH = "../shaders/post/bloom/horz_blur.spv";
constexpr std::string_view VERT_BLUR_PATH = "../shaders/post/bloom/vert_blur.spv";
constexpr std::string_view GATHER_BLUR_PATH = "../shaders/post/bloom/gather.spv";

}

BloomPass add_bloom( vk::Common& vulkan, const State& engine, TaskList& task_list,
    const RWImage& input, const RWImage& output,
    const UniformBuffer<ub_data::Debug>& uniform_debug_buffer )
{
    BloomPass pass = {
        .brightness_threshold = engine::create_rwimage( vulkan, engine,
            { engine.swapchain.extent.width, engine.swapchain.extent.height, 1 },
            VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TYPE_2D, VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
                | VK_IMAGE_USAGE_TRANSFER_SRC_BIT ),
        .horz_blur = engine::create_rwimage( vulkan, engine,
            { engine.swapchain.extent.width, engine.swapchain.extent.height, 1 },
            VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TYPE_2D, VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
                | VK_IMAGE_USAGE_TRANSFER_SRC_BIT ),
    };

    engine::add_pipeline_barrier( task_list,
        { .image_barriers = {
              {
                  .src_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                  .src_access = VK_ACCESS_2_NONE,
                  .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                  .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                  .dst_access = VK_ACCESS_2_SHADER_WRITE_BIT,
                  .dst_layout = VK_IMAGE_LAYOUT_GENERAL,
                  .image = pass.brightness_threshold,
                  .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
              },
              {
                  .src_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                  .src_access = VK_ACCESS_2_NONE,
                  .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                  .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                  .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                  .dst_layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                  .image = pass.horz_blur,
                  .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
              },
          } } );

    {
        engine::DescriptorSet uniform_desc_set = engine::generate_descriptor_set(
            vulkan, engine, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, VK_SHADER_STAGE_COMPUTE_BIT );
        engine::update_descriptor_set_uniform(
            vulkan, engine, uniform_desc_set, uniform_debug_buffer, 0 );

        pass.uniform_desc_set
            = std::make_unique<engine::DescriptorSet>( std::move( uniform_desc_set ) );
    }

    glm::ivec3 dims = {
        ( engine.swapchain.extent.width + 7 ) / 8,
        ( engine.swapchain.extent.height + 7 ) / 8,
        1,
    };

    {
        engine::DescriptorSet brightness_threshold_desc_set = engine::generate_descriptor_set(
            vulkan, engine, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
            VK_SHADER_STAGE_COMPUTE_BIT );
        engine::update_descriptor_set_rwimage( vulkan, engine, brightness_threshold_desc_set, input,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0 );
        engine::update_descriptor_set_rwimage( vulkan, engine, brightness_threshold_desc_set,
            pass.brightness_threshold, VK_IMAGE_LAYOUT_GENERAL, 1 );

        engine::Pipeline brightness_threshold_pipeline = engine::create_compute_pipeline( vulkan,
            { brightness_threshold_desc_set.layouts[0], pass.uniform_desc_set->layouts[0] },
            vk::create::shader_module( vulkan, BRIGHTNESS_THRESHOLD_PATH ), "cs_main" );

        pass.brightness_threshold_desc_set
            = std::make_unique<engine::DescriptorSet>( std::move( brightness_threshold_desc_set ) );

        engine::add_cs_task( task_list,
            {
                .pipeline = brightness_threshold_pipeline,
                .descriptor_sets
                = { pass.brightness_threshold_desc_set.get(), pass.uniform_desc_set.get() },
                .group_size = dims,
            } );
    }

    VkShaderModule horz_blur_shader = vk::create::shader_module( vulkan, HORZ_BLUR_PATH );
    VkShaderModule vert_blur_shader = vk::create::shader_module( vulkan, VERT_BLUR_PATH );

    for ( size_t i = 0; i < 5; ++i ) {
        engine::transition_cs_read_to_write( task_list, pass.horz_blur );
        engine::transition_cs_write_to_read( task_list, pass.brightness_threshold );

        {
            engine::DescriptorSet horz_blur_desc_set = engine::generate_descriptor_set( vulkan,
                engine, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
                VK_SHADER_STAGE_COMPUTE_BIT );
            engine::update_descriptor_set_rwimage( vulkan, engine, horz_blur_desc_set,
                pass.brightness_threshold, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0 );
            engine::update_descriptor_set_rwimage(
                vulkan, engine, horz_blur_desc_set, pass.horz_blur, VK_IMAGE_LAYOUT_GENERAL, 1 );

            engine::Pipeline horz_blur_pipeline = engine::create_compute_pipeline( vulkan,
                { horz_blur_desc_set.layouts[0], pass.uniform_desc_set->layouts[0] },
                horz_blur_shader, "cs_main" );

            pass.horz_blur_desc_sets[i]
                = std::make_unique<engine::DescriptorSet>( std::move( horz_blur_desc_set ) );

            engine::add_cs_task( task_list,
                {
                    .pipeline = horz_blur_pipeline,
                    .descriptor_sets
                    = { pass.horz_blur_desc_sets[i].get(), pass.uniform_desc_set.get() },
                    .group_size = dims,
                } );
        }

        engine::transition_cs_write_to_read( task_list, pass.horz_blur );
        engine::transition_cs_read_to_write( task_list, pass.brightness_threshold );

        {
            engine::DescriptorSet vert_blur_desc_set = engine::generate_descriptor_set( vulkan,
                engine, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
                VK_SHADER_STAGE_COMPUTE_BIT );
            engine::update_descriptor_set_rwimage( vulkan, engine, vert_blur_desc_set,
                pass.horz_blur, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0 );
            engine::update_descriptor_set_rwimage( vulkan, engine, vert_blur_desc_set,
                pass.brightness_threshold, VK_IMAGE_LAYOUT_GENERAL, 1 );

            engine::Pipeline vert_blur_pipeline = engine::create_compute_pipeline( vulkan,
                { vert_blur_desc_set.layouts[0], pass.uniform_desc_set->layouts[0] },
                vert_blur_shader, "cs_main" );

            pass.vert_blur_desc_sets[i]
                = std::make_unique<engine::DescriptorSet>( std::move( vert_blur_desc_set ) );

            engine::add_cs_task( task_list,
                {
                    .pipeline = vert_blur_pipeline,
                    .descriptor_sets
                    = { pass.vert_blur_desc_sets[i].get(), pass.uniform_desc_set.get() },
                    .group_size = dims,
                } );
        }
    }

    // Brightness threshold texture holds the final blurred texture
    engine::transition_cs_write_to_read( task_list, pass.brightness_threshold );

    {
        engine::DescriptorSet gather_desc_set = engine::generate_descriptor_set( vulkan, engine,
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
            VK_SHADER_STAGE_COMPUTE_BIT );
        engine::update_descriptor_set_rwimage( vulkan, engine, gather_desc_set,
            pass.brightness_threshold, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0 );
        engine::update_descriptor_set_rwimage(
            vulkan, engine, gather_desc_set, input, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1 );
        engine::update_descriptor_set_rwimage(
            vulkan, engine, gather_desc_set, output, VK_IMAGE_LAYOUT_GENERAL, 2 );

        engine::Pipeline gather_pipeline = engine::create_compute_pipeline( vulkan,
            { gather_desc_set.layouts[0], pass.uniform_desc_set->layouts[0] },
            vk::create::shader_module( vulkan, GATHER_BLUR_PATH ), "cs_main" );

        pass.gather_desc_set
            = std::make_unique<engine::DescriptorSet>( std::move( gather_desc_set ) );

        engine::add_cs_task( task_list,
            {
                .pipeline = gather_pipeline,
                .descriptor_sets = { pass.gather_desc_set.get(), pass.uniform_desc_set.get() },
                .group_size = dims,
            } );
    }

    log::info( "[Post] Added bloom pass!" );

    return pass;
}

}
