#pragma once

#include "engine/descriptor_set.hpp"
#include "engine/pipeline.hpp"
#include "engine/rwimage.hpp"
#include "engine/task_list.hpp"
#include "engine/ub_data.hpp"
#include "engine/uniform_buffer.hpp"
#include "volumetrics.hpp"

namespace racecar::atmosphere {

struct SunVisibilityComputePass {
    volumetric::Volumetric* volumetric = nullptr;
    engine::DescriptorSet* gamestate_desc_set = nullptr;

    engine::RWImage sun_visibility_shadow_map;
    UniformBuffer<ub_data::TerrainSizeData> terrain_size_data_uniform;

    engine::DescriptorSet visibility_write_desc_set;
    engine::DescriptorSet visibility_read_desc_set;

    engine::Pipeline cs_sun_visibility_pipeline;
};

void initialize_sun_visibility( SunVisibilityComputePass& sun_visibility,
    volumetric::Volumetric& volumetric, engine::DescriptorSet& gamestate_desc_set );

void compute_sun_visibility(
    SunVisibilityComputePass& sun_visibility, engine::TaskList& task_list );

}
