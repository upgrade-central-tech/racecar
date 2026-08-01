#pragma once

#include "../atmosphere_baker.hpp"
#include "../context.hpp"
#include "../engine/descriptor_set.hpp"
#include "../engine/state.hpp"
#include "../engine/ub_data.hpp"
#include "../engine/uniform_buffer.hpp"
#include "../geometry/scene_mesh.hpp"
#include "../vk/ray_tracing.hpp"
#include "scene.hpp"

#include <vector>

namespace racecar {

void alloc_blases( Context& ctx, engine::State& engine,
    const std::vector<const scene::Primitive*>& prims, const geometry::scene::Mesh& scene_mesh,
    ub_data::BLASOffsets* ret_blas_offsets );

void create_objects( engine::State& engine, std::vector<glm::mat4>& transforms,
    std::vector<vk::rt::Object>& objects );

engine::DescriptorSet create_accel_structure_desc_set(
    vk::Common& vulkan, const engine::State& engine );

void alloc_car_tlas(
    vk::Common& vulkan, engine::State& engine, const std::vector<vk::rt::Object>& objects );

void build_car_blases( engine::State& engine, VkCommandBuffer& precompute_cmdbuf );

void build_car_tlas( engine::State& engine, VkCommandBuffer& precompute_cmdbuf );

vk::mem::AllocatedBuffer create_padded_vertex_data_buffer(
    Context& ctx, engine::State& engine, geometry::scene::Mesh& scene_mesh );

engine::DescriptorSet create_car_desc_set( Context& ctx, engine::State& engine,
    geometry::scene::Mesh& scene_mesh, vk::mem::AllocatedBuffer& padded_vertex_data_buffer,
    UniformBuffer<ub_data::BLASOffsets>& offset_data, vk::mem::AllocatedImage& lut_brdf,
    atmosphere::AtmosphereBaker& atms_baker,
    UniformBuffer<ub_data::RTTextureUniform>& rt_texture_uniform_data );

engine::DescriptorSet create_combined_textures_desc_set( Context& ctx, engine::State& engine,
    std::vector<vk::mem::AllocatedImage>& albedo_textures,
    std::vector<vk::mem::AllocatedImage>& metallic_roughness_textures );

void update_rt_uniform_buffers( Context& ctx, engine::State& engine,
    UniformBuffer<ub_data::BLASOffsets>& offset_data,
    UniformBuffer<ub_data::RTTextureUniform>& rt_texture_uniform_data );

}
