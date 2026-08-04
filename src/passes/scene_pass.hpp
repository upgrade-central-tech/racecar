#pragma once

#include "../context.hpp"
#include "../deferred.hpp"
#include "../engine/descriptor_set.hpp"
#include "../engine/gfx_task.hpp"
#include "../engine/pipeline.hpp"
#include "../engine/state.hpp"
#include "../geometry/scene_mesh.hpp"
#include "../scene/scene.hpp"

#include <vector>

namespace racecar {

// One pipeline writing into one GfxTask, with the descriptor sets
// that apply to every prim drawn into it.
struct ScenePassTarget {
    engine::Pipeline& pipeline;
    engine::GfxTask& gfx_task;
    engine::DescriptorSet& uniform_desc_set;
    engine::DescriptorSet& sampler_desc_set;
    engine::DescriptorSet& lut_sets;
    std::vector<engine::DescriptorSet>& material_desc_sets; // index prim->material_id
    std::vector<engine::DescriptorSet>& model_mat_desc_sets; // index prim->node_id
};

struct DepthPassTarget {
    engine::Pipeline& pipeline;
    engine::GfxTask& gfx_task;
    engine::DescriptorSet& uniform_desc_set;
};

void create_scene_gfx_pipeline( 
    engine::Pipeline* scene_pipeline, const geometry::scene::Mesh& scene_mesh,
    engine::DescriptorSet* uniform_desc_set, engine::DescriptorSet* material_desc_sets,
    engine::DescriptorSet* model_mat_desc_sets, engine::DescriptorSet* lut_sets,
    engine::DescriptorSet* sampler_desc_set );

engine::GfxTask create_prepass_gfx_task( deferred::GBuffers& gbuffers );

/*
 * Create a draw task for each prim and add it to the scene pass gfx task and the depth
 * ms gfx task, including all the necessary descriptor sets.
 */
void add_prim_draw_tasks( geometry::scene::Mesh& scene_mesh,
    const std::vector<const scene::Primitive*>& prims, ScenePassTarget scene_pass,
    DepthPassTarget depth_pass );

}
