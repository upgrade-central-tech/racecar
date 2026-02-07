#pragma once

#include "../atmosphere_baker.hpp"
#include "../deferred.hpp"
#include "../engine/prepass.hpp"
#include "../engine/task_list.hpp"
#include "../engine/ub_data.hpp"
#include "../engine/uniform_buffer.hpp"

namespace racecar::geometry {

// Keep it simple for now
struct TerrainVertex {
    glm::vec3 position;
    glm::vec3 normal;
};

struct TerrainLightingInfo {
    UniformBuffer<ub_data::Camera>* camera_buffer;
    UniformBuffer<ub_data::Debug>* debug_buffer;

    atmosphere::AtmosphereBaker* atmosphere_baker;

    deferred::GBuffers* gbuffers;
    // engine::RWImage* GBuffer_Position;
    // engine::RWImage* GBuffer_Normal;
    // engine::RWImage* GBuffer_Albedo;
    // engine::RWImage* GBuffer_Packed_Data;
    engine::RWImage* color_attachment;
    vk::mem::AllocatedImage* lut_brdf;
};

struct TerrainPrepassInfo {
    UniformBuffer<ub_data::Camera>* camera_buffer;
    UniformBuffer<ub_data::Debug>* debug_buffer;

    deferred::GBuffers* gbuffers;

    // engine::RWImage* GBuffer_Position;
    // engine::RWImage* GBuffer_Normal;
    // engine::RWImage* GBuffer_Albedo;
    // engine::RWImage* GBuffer_Depth;
    // engine::RWImage* GBuffer_Packed_Data;
    // engine::RWImage* GBuffer_Velocity;

    vk::mem::AllocatedImage* glint_noise;
};

struct Terrain {
    std::vector<TerrainVertex> vertices;
    std::vector<uint32_t> indices;
    geometry::GPUMeshBuffers mesh_buffers;

    std::vector<uint32_t> tri_indices;
    geometry::GPUMeshBuffers tri_buffers;

    engine::DescriptorSet prepass_uniform_desc_set;
    engine::DescriptorSet prepass_texture_desc_set;
    engine::DescriptorSet prepass_sampler_desc_set;
    engine::DescriptorSet prepass_lut_desc_set;

    engine::DescriptorSet uniform_desc_set;
    engine::DescriptorSet texture_desc_set;
    engine::DescriptorSet lut_desc_set;
    engine::DescriptorSet sampler_desc_set;
    engine::DescriptorSet* accel_structure_desc_set;
    engine::DescriptorSet* reflection_texture_desc_set;

    engine::GfxTask terrain_prepass_task;

    UniformBuffer<ub_data::TerrainData> terrain_uniform;
    vk::rt::AccelerationStructure blas;
    vk::rt::AccelerationStructure tlas;

    // Crap-ton of images. We need a bindless-texture solution or something.
    // Maybe one giant atlas will work, actually.
    vk::mem::AllocatedImage test_layer_mask;
    vk::mem::AllocatedImage grass_albedo_roughness;
    vk::mem::AllocatedImage grass_normal_ao;
    vk::mem::AllocatedImage asphalt_albedo_roughness;
    vk::mem::AllocatedImage asphalt_normal_ao;

    vk::mem::AllocatedImage terrain_noise;

    VkVertexInputBindingDescription vertex_binding_description = {
        .binding = vk::binding::VERTEX_BUFFER,
        .stride = sizeof( TerrainVertex ),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions = { {
        { 0, vk::binding::VERTEX_BUFFER, VK_FORMAT_R32G32B32_SFLOAT,
            offsetof( TerrainVertex, position ) },
        { 1, vk::binding::VERTEX_BUFFER, VK_FORMAT_R32G32B32_SFLOAT,
            offsetof( TerrainVertex, normal ) },
    } };
};

void initialize_terrain( vk::Common& vulkan, engine::State& engine, Terrain& terrain );

void draw_terrain_prepass( Terrain& terrain, vk::Common& vulkan, engine::State& engine,
    const TerrainPrepassInfo& prepass_info,
    [[maybe_unused]] engine::DepthPrepassMS& depth_prepass_ms_task, engine::TaskList& task_list );

void draw_terrain( Terrain& terrain, vk::Common& vulkan, engine::State& engine,
    engine::TaskList& task_list, TerrainLightingInfo& info );

}
