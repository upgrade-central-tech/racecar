#include "anti_aliasing.hpp"

#include "../../vk/create.hpp"

#include <string_view>

namespace racecar::engine::post {

static constexpr std::string_view ANTI_ALIASING_SHADER_PATH = "../shaders/post/aa/aa.spv";

AAPass add_aa( vk::Common& vulkan, const State& engine, const RWImage& input, const RWImage& output,
    const RWImage& history, TaskList& task_list )
{

    AAPass pass;

    {
        engine::DescriptorSet uniform_desc_set = engine::generate_descriptor_set( vulkan, engine,
            {
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            },
            VK_SHADER_STAGE_COMPUTE_BIT );

        engine::update_descriptor_set_rwimage(
            vulkan, engine, uniform_desc_set, input, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0 );
        engine::update_descriptor_set_rwimage( vulkan, engine, uniform_desc_set, history,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1 );
        engine::update_descriptor_set_rwimage(
            vulkan, engine, uniform_desc_set, output, VK_IMAGE_LAYOUT_GENERAL, 2 );

        pass.uniform_desc_set
            = std::make_unique<engine::DescriptorSet>( std::move( uniform_desc_set ) );
    }

    engine::add_cs_task( task_list,
        {
            .pipeline
            = engine::create_compute_pipeline( vulkan, { pass.uniform_desc_set->layouts[0] },
                vk::create::shader_module( vulkan, ANTI_ALIASING_SHADER_PATH ), "cs_main" ),
            .descriptor_sets = { pass.uniform_desc_set.get() },
            .group_size = glm::ivec3( ( engine.swapchain.extent.width + 7 ) / 8,
                ( engine.swapchain.extent.height + 7 ) / 8, 1 ),
        } );

    return pass;
}

}
