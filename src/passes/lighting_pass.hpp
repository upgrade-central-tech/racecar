#pragma once

#include "../context.hpp"
#include "../deferred.hpp"
#include "../engine/descriptor_set.hpp"
#include "../engine/pipeline.hpp"
#include "../engine/state.hpp"
#include "../engine/task_list.hpp"

#include <vector>

namespace racecar {

struct LightingPassDescSets {
    engine::DescriptorSet& uniform_desc_set;
    std::vector<engine::DescriptorSet>& material_desc_sets;
    engine::DescriptorSet& lut_sets;
    engine::DescriptorSet& sampler_desc_set;
    engine::DescriptorSet& gbuffer_desc_set;
    engine::DescriptorSet& car_tlas_desc_set;
    engine::DescriptorSet& reflection_buffer_desc_set;
};

void create_lighting_pass_resources( Context& ctx, engine::State& engine,
    LightingPassDescSets desc_sets, engine::Pipeline* lighting_pass_pipeline );

void car_lighting_pass( engine::State& engine, LightingPassDescSets desc_sets,
    engine::Pipeline& lighting_pass_pipeline, engine::RWImage& screen_color,
    engine::TaskList& task_list );

void create_deferred_lighting_pipeline_barrier( engine::TaskList& task_list,
    deferred::GBuffers& gbuffers, engine::RWImage& reflection_data, engine::RWImage& screen_color );

/*
 * Hands screen_color over from the terrain lighting compute pass, which writes it as a storage
 * image, to the car lighting pass, which renders into it as a color attachment.
 */
void create_terrain_car_screen_pipeline_barrier(
    engine::TaskList& task_list, engine::RWImage& screen_color );

}
