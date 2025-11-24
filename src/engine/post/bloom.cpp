#include "bloom.hpp"

#include "../../vk/create.hpp"

namespace racecar::engine::post {

namespace {

constexpr std::string_view BRIGHTNESS_THRESHOLD_PATH
    = "../shaders/post/bloom/brightness_threshold.spv";

}

BloomPass add_bloom( vk::Common& vulkan, const State& engine, TaskList& task_list,
    const RWImage& input, const RWImage& output )
{
    BloomPass pass;

    glm::ivec2 dims = {
        ( engine.swapchain.extent.width + 7 ) / 8,
        ( engine.swapchain.extent.height + 7 ) / 8,
    };

    {
        engine::DescriptorSet brightness_threshold_desc_set = engine::generate_descriptor_set(
            vulkan, engine, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
            VK_SHADER_STAGE_COMPUTE_BIT );
        engine::update_descriptor_set_rwimage( vulkan, engine, brightness_threshold_desc_set, input,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0 );
        engine::update_descriptor_set_rwimage(
            vulkan, engine, brightness_threshold_desc_set, output, VK_IMAGE_LAYOUT_GENERAL, 1 );

        engine::Pipeline brightness_threshold_pipeline
            = engine::create_compute_pipeline( vulkan, { brightness_threshold_desc_set.layouts[0] },
                vk::create::shader_module( vulkan, BRIGHTNESS_THRESHOLD_PATH ), "cs_main" );

        pass.brightness_threshold_desc_set
            = std::make_unique<engine::DescriptorSet>( std::move( brightness_threshold_desc_set ) );

        engine::add_cs_task( task_list,
            {
                .pipeline = brightness_threshold_pipeline,
                .descriptor_sets = { pass.brightness_threshold_desc_set.get() },
                .group_size = glm::ivec3( dims, 1 ),
            } );
    }

    return pass;
}

}
