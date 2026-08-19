#pragma once

#include "engine/descriptor_set.hpp"
#include "engine/pipeline.hpp"
#include "engine/task_list.hpp"
#include "volumetrics.hpp"

namespace racecar::atmosphere {

struct SunVisibilityComputePass {
    volumetric::Volumetric* volumetric = nullptr;

    std::vector<vk::mem::AllocatedBuffer> visibility_buffer;

    engine::DescriptorSet visibility_desc_set;

    engine::Pipeline cs_sun_visibility_pipeline;
};

void initialize_sun_visibility(
    SunVisibilityComputePass& sun_visibility, volumetric::Volumetric& volumetric );

void compute_sun_visibility(
    SunVisibilityComputePass& sun_visibility, engine::TaskList& task_list );

}
