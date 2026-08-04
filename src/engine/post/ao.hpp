#pragma once

#include "../task_list.hpp"
#include "../ub_data.hpp"

namespace racecar::gui {
struct Gui;
}

namespace racecar::engine::post {

struct AoPass {
    UniformBuffer<ub_data::Camera>* camera_buffer;

    engine::RWImage* GBuffer_Normal;
    engine::RWImage* GBuffer_Depth;
    engine::RWImage* in_color;
    engine::RWImage* out_color;

    engine::DescriptorSet uniform_desc_set = {};
    engine::DescriptorSet texture_desc_set = {};
    UniformBuffer<ub_data::AOData> ao_buffer = {};
};

void add_ao( AoPass& ao_pass, TaskList& task_list );

void update_ao_uniform_buffer(
    const gui::Gui& gui, engine::post::AoPass& ao_pass );

}
