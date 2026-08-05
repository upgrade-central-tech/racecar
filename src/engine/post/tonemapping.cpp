#include "tonemapping.hpp"

#include "../../gui.hpp"
#include "../../vk/create.hpp"
#include "../../settings.h"

#include <string_view>

namespace racecar::engine::post {

static constexpr std::string_view TONEMAPPING_SHADER_PATH
    = "../shaders/post/tonemapping/tonemapping.spv";

TonemappingPass add_tonemapping( const RWImage& input, const RWImage& output, TaskList& task_list )
{
    const engine::State& engine = engine::State::GetConst();

    TonemappingPass pass;
    pass.buffer = create_uniform_buffer<ub_data::Tonemapping>( { }, engine.frame_overlap );

    {
        engine::DescriptorSet uniform_desc_set = engine::generate_descriptor_set(
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
              VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
            VK_SHADER_STAGE_COMPUTE_BIT
        );
        engine::update_descriptor_set_rwimage(
            uniform_desc_set,
            input,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            0
        );
        engine::update_descriptor_set_rwimage(
            uniform_desc_set,
            output,
            VK_IMAGE_LAYOUT_GENERAL,
            1
        );
        engine::update_descriptor_set_uniform( uniform_desc_set, pass.buffer, 2 );

        pass.uniform_desc_set
            = std::make_unique<engine::DescriptorSet>( std::move( uniform_desc_set ) );
    }

    engine::add_cs_task(
        task_list,
        {
            .pipeline = engine::create_compute_pipeline(
                { pass.uniform_desc_set->layouts[0] },
                vk::create::shader_module( TONEMAPPING_SHADER_PATH ),
                "cs_main"
            ),
            .descriptor_sets = { pass.uniform_desc_set.get() },
            .group_size = glm::ivec3(
                ( engine.swapchain.extent.width + 7 ) / 8,
                ( engine.swapchain.extent.height + 7 ) / 8,
                1
            ),
        }
    );

    return pass;
}

void update_tonemapping_uniform_buffer(
    const gui::Gui& gui, engine::post::TonemappingPass& tm_pass
)
{
    const engine::State& engine = engine::State::GetConst();
    ub_data::Tonemapping tm_ub = tm_pass.buffer.get_data();
    tm_ub.mode = static_cast<int>( RuntimeSettings::GetValue( RacecarSettings::TONEMAPPING_MODE ) );
    tm_ub.hdr_target_luminance = gui.tonemapping.hdr_target_luminance;

    tm_pass.buffer.set_data( tm_ub );
    tm_pass.buffer.update( engine.get_frame_index() );
}

}
