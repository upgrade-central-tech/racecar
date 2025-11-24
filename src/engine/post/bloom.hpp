#pragma once

#include "../../vk/common.hpp"
#include "../descriptor_set.hpp"
#include "../rwimage.hpp"
#include "../state.hpp"
#include "../task_list.hpp"

#include <array>
#include <memory>

namespace racecar::engine::post {

struct BloomPass {
    RWImage brightness_threshold;
    RWImage horz_blur;

    std::unique_ptr<engine::DescriptorSet> brightness_threshold_desc_set;
    std::array<std::unique_ptr<engine::DescriptorSet>, 5> horz_blur_desc_sets;
    std::array<std::unique_ptr<engine::DescriptorSet>, 5> vert_blur_desc_sets;
};

BloomPass add_bloom( vk::Common& vulkan, const State& engine, TaskList& task_list,
    const RWImage& input, const RWImage& output );

}
