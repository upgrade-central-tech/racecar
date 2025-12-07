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
    static constexpr size_t NUM_PASSES = 5;
    static constexpr size_t NUM_SAMPLES = ( 2 * NUM_PASSES ) - 1;

    /// A separate image is allocated for each downsample and upsample, except for the last
    /// downsample and the first upsample: these share the same image.
    std::array<RWImage, NUM_SAMPLES> samples;

    std::array<std::unique_ptr<engine::DescriptorSet>, NUM_PASSES> downsample_desc_sets;
    std::array<std::unique_ptr<engine::DescriptorSet>, NUM_PASSES> upsample_desc_sets;

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
