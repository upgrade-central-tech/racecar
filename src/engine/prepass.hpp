#pragma once

#include "../context.hpp"
#include "../deferred.hpp"
#include "../geometry/scene_mesh.hpp"
#include "descriptor_set.hpp"
#include "state.hpp"
#include "ub_data.hpp"
#include "uniform_buffer.hpp"
#include "gfx_task.hpp"
#include "pipeline.hpp"

namespace racecar::engine {

struct DepthPrepassMS {
    engine::GfxTask depth_ms_gfx_task;

    std::vector<DescriptorSet*> descriptor_sets;
    engine::Pipeline pipeline;
};

void PushDepthPrepassMS(
    DepthPrepassMS& depth_prepass_ms, engine::DrawResourceDescriptor draw_descriptor );

GfxTask create_depth_ms_gfx_task( engine::State& engine, deferred::GBuffers* gbuffers );

void create_depth_ms_prepass( Context& ctx, engine::State& engine,
    engine::DescriptorSet* depth_uniform_desc_set, engine::Pipeline* depth_ms_pipeline,
    const UniformBuffer<ub_data::Camera>& camera_buffer,
    const geometry::scene::Mesh& scene_mesh, deferred::GBuffers* gbuffers,
    engine::DepthPrepassMS* depth_prepass_ms );

}
