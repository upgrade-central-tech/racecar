#pragma once

#include "engine/descriptor_set.hpp"
#include "engine/rwimage.hpp"
#include "engine/task_list.hpp"
#include "engine/ub_data.hpp"
#include "engine/uniform_buffer.hpp"

#include <vector>

namespace racecar {

constexpr uint32_t MAX_DEBUG_TEXTURES = 8;

struct DebugTexturePass {
    engine::DescriptorSet texture_desc_set;
    engine::DescriptorSet output_desc_set;
    engine::Pipeline pipeline;
    UniformBuffer<ub_data::DebugTexture> buffer;
    bool initialized = false;
};

void initialize_debug_texture_pass( DebugTexturePass& pass, engine::RWImage& screen_buffer,
    const std::vector<const engine::RWImage*>& textures );

void add_debug_texture_pass( DebugTexturePass& pass, engine::TaskList& task_list );

void update_debug_texture_uniform_buffer( DebugTexturePass& pass, float exposure );

}
