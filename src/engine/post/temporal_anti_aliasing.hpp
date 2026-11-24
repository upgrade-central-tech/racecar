#pragma once

#include "../../engine/ub_data.hpp"
#include "../../vk/common.hpp"
#include "../descriptor_set.hpp"
#include "../image.h"
#include "../state.hpp"
#include "../task_list.hpp"

#include <memory>

namespace racecar::engine::post {

struct TAAPass {
    std::unique_ptr<DescriptorSet> uniform_desc_set;
    std::unique_ptr<DescriptorSet> history_desc_set;

    UniformBuffer<ub_data::AA> buffer;
};

TAAPass add_taa(
    const Image& input,
    const Image& GBuffer_Depth,
    const Image& GBuffer_Velocity,
    Image& output,
    Image& history,
    TaskList& task_list,
    UniformBuffer<ub_data::Camera>& camera_buffer
);

void update_aa_uniform_buffer( engine::post::TAAPass& taa_pass );

}
