#pragma once

#include "../../vk/common.hpp"
#include "../descriptor_set.hpp"
#include "../rwimage.hpp"
#include "../state.hpp"
#include "../task_list.hpp"

#include <memory>

namespace racecar::engine::post {

struct AAPass {
    std::unique_ptr<DescriptorSet> uniform_desc_set;
    std::unique_ptr<DescriptorSet> history_desc_set;
};

AAPass add_aa( vk::Common& vulkan, State& engine, const RWImage& input, RWImage& output,
    RWImage& history, TaskList& task_list );

}
