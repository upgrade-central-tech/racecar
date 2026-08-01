#pragma once

#include "context.hpp"
#include "engine/descriptor_set.hpp"
#include "engine/rwimage.hpp"
#include "engine/state.hpp"
#include "engine/ub_data.hpp"
#include "engine/uniform_buffer.hpp"
#include "geometry/scene_mesh.hpp"
#include "scene/scene.hpp"

#include <vector>

namespace racecar {

Context initialize_context( bool use_fullscreen );

void load_scene(
    Context& ctx, engine::State& engine, scene::Scene* scene, geometry::scene::Mesh* scene_mesh );

void load_camera_debug_uniform_buffers( Context& ctx, engine::State& engine,
    UniformBuffer<ub_data::Camera>* camera_buffer, UniformBuffer<ub_data::Debug>* debug_buffer,
    engine::DescriptorSet* uniform_desc_set );

void load_samplers( Context& ctx, engine::State& engine, VkSampler* linear_sampler,
    VkSampler* point_sampler, engine::DescriptorSet* sampler_desc_set );

void load_materials( Context& ctx, engine::State& engine, scene::Scene& scene, size_t num_materials,
    std::vector<engine::DescriptorSet>* material_desc_sets,
    std::vector<UniformBuffer<ub_data::Material>>* material_uniform_buffers );

void load_model_mat_uniform_buffers( Context& ctx, engine::State& engine, size_t num_nodes,
    scene::Scene* scene, std::vector<engine::DescriptorSet>* model_mat_desc_sets,
    std::vector<UniformBuffer<ub_data::ModelMat>>* model_mat_uniform_buffers );

void create_raymarch_tex_sets(
    Context& ctx, engine::State& engine, engine::DescriptorSet* raymarch_tex_sets );

void create_screen_buffers( Context& ctx, engine::State& engine, engine::RWImage* screen_color,
    engine::RWImage* screen_buffer, engine::RWImage* screen_history );

void load_model_primitive_material_data( Context& ctx, engine::State& engine,
    const scene::Scene& scene,
    const std::vector<UniformBuffer<ub_data::ModelMat>>& model_mat_uniform_buffers,
    std::vector<glm::mat4>& transforms, std::vector<const scene::Primitive*>& prims,
    ub_data::RTTextureUniform& rt_texture_uniform,
    std::vector<vk::mem::AllocatedImage>& albedo_textures,
    std::vector<vk::mem::AllocatedImage>& metallic_roughness_textures,
    std::vector<engine::DescriptorSet>& material_desc_sets );

}
