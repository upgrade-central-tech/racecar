#pragma once

#if RACECAR_RAY_TRACING

#include "../context.hpp"
#include "../deferred.hpp"
#include "../engine/descriptor_set.hpp"
#include "../engine/gfx_task.hpp"
#include "../engine/pipeline.hpp"
#include "../engine/state.hpp"
#include "../engine/task_list.hpp"

namespace racecar {

struct ReflectionPassDescSets {
    engine::DescriptorSet& uniform_desc_set;
    engine::DescriptorSet& sampler_desc_set;
    engine::DescriptorSet& gbuffer_desc_set;
    engine::DescriptorSet& car_tlas_desc_set;
    engine::DescriptorSet& car_desc_set;
    engine::DescriptorSet& combined_textures_desc_set;
    engine::DescriptorSet& terrain_shading_desc_set;
};

void create_reflection_pass_resources( 
    ReflectionPassDescSets desc_sets, engine::RWImage* reflection_data,
    engine::Pipeline* reflection_pipeline, engine::DescriptorSet* reflection_buffer_desc_set,
    engine::GfxTask* reflection_gfx_task );

void create_deferred_reflection_pipeline_barrier(
    engine::TaskList& task_list, deferred::GBuffers& gbuffers, engine::RWImage& reflection_data );

}

#endif // RACECAR_RAY_TRACING
