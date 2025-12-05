#pragma once

#include "../../vk/common.hpp"
#include "../descriptor_set.hpp"
#include "../rwimage.hpp"
#include "../state.hpp"
#include "../task_list.hpp"
#include "../ub_data.hpp"
#include "../uniform_buffer.hpp"

#include <array>
#include <memory>

namespace racecar::engine::post {

struct BloomPass {
    RWImage brightness_threshold;
    RWImage horz_blur;

    std::unique_ptr<engine::DescriptorSet> brightness_threshold_desc_set;
    std::array<std::unique_ptr<engine::DescriptorSet>, 5> horz_blur_desc_sets;
    std::array<std::unique_ptr<engine::DescriptorSet>, 5> vert_blur_desc_sets;
    std::unique_ptr<engine::DescriptorSet> gather_desc_set;

    std::unique_ptr<engine::DescriptorSet> uniform_desc_set;
    std::unique_ptr<engine::DescriptorSet> sampler_desc_set;
};

/// Assumes that `input` is already in `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` and `output` is
/// already in `VK_IMAGE_LAYOUT_GENERAL` with `VK_ACCESS_2_SHADER_WRITE_BIT`.
BloomPass add_bloom( vk::Common& vulkan, const State& engine, TaskList& task_list,
    const RWImage& input, const RWImage& output,
    const UniformBuffer<ub_data::Debug>& uniform_debug_buffer );

}
