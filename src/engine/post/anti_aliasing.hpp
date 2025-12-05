#pragma once

#include "../../vk/common.hpp"
#include "../descriptor_set.hpp"
#include "../rwimage.hpp"
#include "../state.hpp"
#include "../task_list.hpp"
#include "../ub_data.hpp"
#include "../uniform_buffer.hpp"

#include <memory>

namespace racecar::engine::post {

struct AAPass {
    std::unique_ptr<DescriptorSet> uniform_desc_set;
};

AAPass add_aa( vk::Common& vulkan, const State& engine, const RWImage& input, const RWImage& output,
    const RWImage& history, TaskList& task_list );

}
