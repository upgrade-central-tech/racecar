#pragma once

#include "../engine/descriptor_set.hpp"
#include "../engine/gfx_task.hpp"
#include "../engine/pipeline.hpp"
#include "../engine/task_list.hpp"
#include "../engine/ub_data.hpp"
#include "../engine/uniform_buffer.hpp"
#include "../geometry/scene_mesh.hpp"

#include <glm/glm.hpp>

#include <vector>

namespace racecar {

struct TransparentPrimInfo {
    int32_t index_offset = 0;
    int node_id = -1;
    glm::vec3 centroid = { };
};

struct TransparencyPass {
    engine::Pipeline transparency_pipeline;

    engine::DescriptorSet* uniform_desc_set = nullptr;
    std::vector<engine::DescriptorSet>* material_desc_sets = nullptr;
    std::vector<engine::DescriptorSet>* model_mat_desc_sets = nullptr;
    engine::DescriptorSet* lut_sets = nullptr;
    engine::DescriptorSet* sampler_desc_set = nullptr;

    std::vector<TransparentPrimInfo> prim_info;
};

void create_transparency_pass_resources(
    TransparencyPass* transparency_pass,
    const geometry::scene::Mesh& scene_mesh,
    const std::vector<const scene::Primitive*>& transparent_prims,
    engine::DescriptorSet* uniform_desc_set,
    std::vector<engine::DescriptorSet>* material_desc_sets,
    std::vector<engine::DescriptorSet>* model_mat_desc_sets,
    engine::DescriptorSet* lut_sets,
    engine::DescriptorSet* sampler_desc_set
);

void execute_transparency_pass(
    TransparencyPass* transparency_pass,
    geometry::scene::Mesh& scene_mesh,
    const std::vector<const scene::Primitive*>& transparent_prims,
    engine::RWImage* target_color,
    engine::RWImage* gbuffer_depth_image,
    const UniformBuffer<ub_data::Camera>& camera_buffer,
    const std::vector<UniformBuffer<ub_data::ModelMat>>& model_mat_uniform_buffers,
    engine::TaskList& task_list
);

void create_transparency_pipeline_barrier(
    engine::RWImage& target_color, engine::RWImage& gbuffer_depth, engine::TaskList& task_list
);

}
