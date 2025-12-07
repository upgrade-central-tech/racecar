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

    std::array<RWImage, NUM_PASSES> images;
    UniformBuffer<ub_data::Bloom> bloom_ub;

    std::unique_ptr<engine::DescriptorSet> threshold_desc_set;
    std::array<std::unique_ptr<engine::DescriptorSet>, NUM_PASSES> downsample_desc_sets;
    std::array<std::unique_ptr<engine::DescriptorSet>, NUM_PASSES> upsample_desc_sets;
    std::unique_ptr<engine::DescriptorSet> uniform_desc_set;
    std::unique_ptr<engine::DescriptorSet> sampler_desc_set;
};

/// Assumes that `input` is already in `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`.
BloomPass add_bloom( vk::Common& vulkan, const State& engine, TaskList& task_list, RWImage& inout,
    RWImage& write_only, const UniformBuffer<ub_data::Debug>& uniform_debug_buffer );

}
