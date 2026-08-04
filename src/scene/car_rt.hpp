#pragma once

#if RACECAR_RAY_TRACING

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

void alloc_blases( 
    const std::vector<const scene::Primitive*>& prims, const geometry::scene::Mesh& scene_mesh,
    ub_data::BLASOffsets* ret_blas_offsets );

void create_objects( std::vector<glm::mat4>& transforms,
    std::vector<vk::rt::Object>& objects );

engine::DescriptorSet create_accel_structure_desc_set();

void alloc_car_tlas(
    const std::vector<vk::rt::Object>& objects );

void build_car_blases( VkCommandBuffer& precompute_cmdbuf );

void build_car_tlas( VkCommandBuffer& precompute_cmdbuf );

vk::mem::AllocatedBuffer create_padded_vertex_data_buffer(
    geometry::scene::Mesh& scene_mesh );

engine::DescriptorSet create_car_desc_set( 
    geometry::scene::Mesh& scene_mesh, vk::mem::AllocatedBuffer& padded_vertex_data_buffer,
    UniformBuffer<ub_data::BLASOffsets>& offset_data, vk::mem::AllocatedImage& lut_brdf,
    atmosphere::AtmosphereBaker& atms_baker,
    UniformBuffer<ub_data::RTTextureUniform>& rt_texture_uniform_data );

engine::DescriptorSet create_combined_textures_desc_set( 
    std::vector<vk::mem::AllocatedImage>& albedo_textures,
    std::vector<vk::mem::AllocatedImage>& metallic_roughness_textures );

void update_rt_uniform_buffers( 
    UniformBuffer<ub_data::BLASOffsets>& offset_data,
    UniformBuffer<ub_data::RTTextureUniform>& rt_texture_uniform_data );

}

#endif // RACECAR_RAY_TRACING
