#pragma once

#include "../engine/descriptor_set.hpp"
#include "../engine/gfx_task.hpp"
#include "../engine/pipeline.hpp"
#include "../engine/task_list.hpp"

namespace racecar {

struct TransparencyPass {
    engine::Pipeline transparency_pipeline;

    engine::GfxTask transparency_gfx_task;
    engine::CPUTask depth_sort_cpu_task;

    engine::DescriptorSet* uniform_desc_set;
    std::vector<engine::DescriptorSet>* model_mat_desc_sets;
};

void create_transparency_pass_resources(
    TransparencyPass* transparency_pass,
    const geometry::scene::Mesh& scene_mesh,
    engine::DescriptorSet* uniform_desc_set,
    std::vector<engine::DescriptorSet>* model_mat_desc_sets
);

void execute_transparency_pass(
    TransparencyPass* transparency_pass,
    geometry::scene::Mesh& scene_mesh,
    std::vector<const scene::Primitive*>& transparent_prims,
    engine::RWImage* screen_color,
    engine::RWImage* gbuffer_depth_image,
    engine::TaskList& task_list
);

void create_lighting_transparency_pipeline_barrier(
    engine::RWImage& screen_color,
    engine::RWImage& gbuffer_depth,
    engine::TaskList& task_list
);

}