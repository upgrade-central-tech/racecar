#pragma once

#include "../engine/prepass.hpp"
#include "../engine/task_list.hpp"
#include "../engine/ub_data.hpp"

namespace racecar::geometry {

// Keep it simple for now
struct TerrainVertex {
    glm::vec3 position;
    glm::vec3 normal;
};

struct Terrain {
    std::vector<TerrainVertex> vertices;
    std::vector<uint32_t> indices;
    geometry::GPUMeshBuffers mesh_buffers;

    engine::DescriptorSet prepass_uniform_desc_set;
    engine::DescriptorSet uniform_desc_set;
    engine::DescriptorSet texture_desc_set;
    engine::DescriptorSet sampler_desc_set;

    engine::GfxTask terrain_prepass_task;

    // Crap-ton of images. We need a bindless-texture solution or something.
    // Maybe one giant atlas will work, actually.
    vk::mem::AllocatedImage test_layer_mask;
    vk::mem::AllocatedImage grass_albedo;
    vk::mem::AllocatedImage grass_normal_ao;

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
    const engine::RWImage& GBuffer_Position, const engine::RWImage& GBuffer_Normal,
    const engine::RWImage& depth_image, UniformBuffer<ub_data::Camera>& camera_buffer,
    engine::DepthPrepassMS& depth_prepass_ms_task, engine::TaskList& task_list );

void draw_terrain( Terrain& terrain, vk::Common& vulkan, engine::State& engine,
    UniformBuffer<ub_data::Camera>& camera_buffer, UniformBuffer<ub_data::Debug>& debug_buffer,
    engine::TaskList& task_list, engine::RWImage& GBuffer_Position, engine::RWImage& GBuffer_Normal,
    engine::RWImage& color_attachment );

}
