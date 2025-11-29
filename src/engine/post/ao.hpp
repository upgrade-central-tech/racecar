#pragma once

#include "../task_list.hpp"
#include "../ub_data.hpp"

namespace racecar::engine::post {

struct AoPass {
    UniformBuffer<ub_data::Camera>* camera_buffer;
    engine::RWImage* GBuffer_Normal;
    engine::RWImage* GBuffer_Depth;
    engine::RWImage* in_color;
    engine::RWImage* out_color;

    engine::DescriptorSet uniform_desc_set = {};
    engine::DescriptorSet texture_desc_set = {};
};

void add_ao( AoPass& ao_pass, vk::Common& vulkan, engine::State& engine, TaskList& task_list );

}
