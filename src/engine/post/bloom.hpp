#pragma once

#include "../../vk/common.hpp"
#include "../descriptor_set.hpp"
#include "../rwimage.hpp"
#include "../state.hpp"
#include "../task_list.hpp"

#include <memory>

namespace racecar::engine::post {

struct BloomPass {
    std::unique_ptr<engine::DescriptorSet> brightness_threshold_desc_set;
};

BloomPass add_bloom( vk::Common& vulkan, const State& engine, TaskList& task_list,
    const RWImage& input, const RWImage& output );

}
