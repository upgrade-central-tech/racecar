#include "debug_texture_pass.hpp"

#include "engine/pipeline.hpp"
#include "settings.h"
#include "vk/create.hpp"

#include <string_view>

namespace racecar {

namespace {

constexpr std::string_view DEBUG_TEXTURE_SHADER_PATH = "../shaders/post/debug_texture/debug_texture.spv";

bool debug_texture_is_on()
{
    return RuntimeSettings::Get<RacecarSettings::DEBUG_TEXTURE>() != DebugTexture::NONE;
}

}

void initialize_debug_texture_pass(
    DebugTexturePass& pass,
    engine::RWImage& screen_buffer,
    const std::vector<const engine::RWImage*>& textures
)
{
    const engine::State& engine = engine::State::GetConst();
    const vk::Common& vulkan = vk::Common::GetConst();

    if ( textures.empty() ) {
        return;
    }

    pass.texture_desc_set = engine::generate_array_descriptor_set(
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
        VK_SHADER_STAGE_COMPUTE_BIT,
        MAX_DEBUG_TEXTURES
    );

    pass.output_desc_set = engine::generate_descriptor_set(
        {
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_DESCRIPTOR_TYPE_SAMPLER,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        },
        VK_SHADER_STAGE_COMPUTE_BIT
    );

    pass.buffer = create_uniform_buffer<ub_data::DebugTexture>( { }, engine.frame_overlap );

    engine::update_descriptor_set_rwimage_array(
        pass.texture_desc_set,
        textures,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        0,
        MAX_DEBUG_TEXTURES
    );

    engine::update_descriptor_set_rwimage(
        pass.output_desc_set,
        screen_buffer,
        VK_IMAGE_LAYOUT_GENERAL,
        0
    );
    engine::update_descriptor_set_sampler(
        pass.output_desc_set,
        vulkan.global_samplers.linear_sampler,
        1
    );
    engine::update_descriptor_set_uniform( pass.output_desc_set, pass.buffer, 2 );

    pass.pipeline = engine::create_compute_pipeline(
        {
            pass.texture_desc_set.layouts[0],
            pass.output_desc_set.layouts[0],
        },
        vk::create::shader_module( DEBUG_TEXTURE_SHADER_PATH ),
        "cs_debug_texture"
    );

    pass.initialized = true;
}

void add_debug_texture_pass( DebugTexturePass& pass, engine::TaskList& task_list )
{
    const engine::State& engine = engine::State::GetConst();

    if ( !pass.initialized ) {
        return;
    }

    engine::add_cs_task(
        task_list,
        engine::ComputeTask {
            pass.pipeline,
            {
                &pass.texture_desc_set,
                &pass.output_desc_set,
            },
            glm::ivec3(
                ( engine.swapchain.extent.width + 7 ) / 8,
                ( engine.swapchain.extent.height + 7 ) / 8,
                1
            ),
        },
        &debug_texture_is_on
    );
}

void update_debug_texture_uniform_buffer( DebugTexturePass& pass, float exposure, float blur )
{
    const engine::State& engine = engine::State::GetConst();

    ub_data::DebugTexture ub = pass.buffer.get_data();
    ub.index = RuntimeSettings::GetValue( RacecarSettings::DEBUG_TEXTURE );
    ub.exposure = exposure;
    ub.blur = blur;

    pass.buffer.set_data( ub );
    pass.buffer.update( engine.get_frame_index() );
}

}
