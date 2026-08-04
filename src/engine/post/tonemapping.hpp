#pragma once

#include "../../vk/common.hpp"
#include "../descriptor_set.hpp"
#include "../rwimage.hpp"
#include "../state.hpp"
#include "../task_list.hpp"
#include "../ub_data.hpp"
#include "../uniform_buffer.hpp"

#include <memory>

namespace racecar::gui {
struct Gui;
}

namespace racecar::engine::post {

struct TonemappingPass {
    std::unique_ptr<DescriptorSet> uniform_desc_set;
    UniformBuffer<ub_data::Tonemapping> buffer;
};

TonemappingPass add_tonemapping( const RWImage& input,
    const RWImage& output, TaskList& task_list );

void update_tonemapping_uniform_buffer(
    const gui::Gui& gui, engine::post::TonemappingPass& tm_pass );

}
