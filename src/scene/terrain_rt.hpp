#pragma once

#if RACECAR_RAY_TRACING

#include "../engine/descriptor_set.hpp"
#include "../engine/pipeline.hpp"
#include "../engine/task_list.hpp"
#include "../engine/ub_data.hpp"
#include "../engine/uniform_buffer.hpp"
#include "../terrain/terrain.hpp"
#include "../vk/ray_tracing.hpp"

#include <vector>

// At the moment this is called "Terrain RT", but eventually it will be
// the RT module for any non-car objects (potentially scene obejcts).
// That's why it is independent and not contained within terrain.cpp

namespace racecar {

constexpr uint32_t TERRAIN_RT_VERTS_PER_SIDE = 256;
constexpr uint32_t TERRAIN_RT_QUADS_PER_SIDE = TERRAIN_RT_VERTS_PER_SIDE - 1;
constexpr uint32_t TERRAIN_RT_VERTEX_COUNT = TERRAIN_RT_VERTS_PER_SIDE * TERRAIN_RT_VERTS_PER_SIDE;
constexpr uint32_t TERRAIN_RT_TRIANGLE_COUNT
    = TERRAIN_RT_QUADS_PER_SIDE * TERRAIN_RT_QUADS_PER_SIDE * 2;
constexpr uint32_t TERRAIN_RT_INDEX_COUNT = TERRAIN_RT_TRIANGLE_COUNT * 3;
constexpr float TERRAIN_RT_WORLD_HALF_WIDTH = geometry::TERRAIN_HALF_WIDTH;

// gpu vertex data format
struct TerrainRTVertex {
    glm::vec3 position;
    float _pad0;
    glm::vec3 normal;
    float _pad1;
};

// uniform buffer struct
struct TerrainRTGrid {
    glm::vec2 world_min;
    glm::vec2 world_size;
    glm::uvec2 vertex_count;
    glm::uvec2 _pad;
};

struct TerrainRayTracingInfo {
    std::vector<racecar::vk::rt::AccelerationStructure> tlas;

    std::vector<vk::rt::AccelerationStructure> dynamic_terrain_blas;
    std::vector<vk::mem::AllocatedBuffer> dynamic_terrain_mesh_buffer;

    vk::mem::AllocatedBuffer terrain_index_buffer = { };

    engine::DescriptorSet tlas_desc_set = { };
    engine::DescriptorSet shading_desc_set = { };

    // borrowed from the raster terrain
    geometry::Terrain* terrain = nullptr;
    UniformBuffer<ub_data::TerrainData>* terrain_uniform = nullptr;
    vk::mem::AllocatedImage* layer_mask = nullptr;

    UniformBuffer<TerrainRTGrid> grid_uniform = { };

    engine::DescriptorSet uniform_desc_set = { };
    engine::DescriptorSet texture_desc_set = { };
    engine::DescriptorSet sampler_desc_set = { };
    engine::DescriptorSet mesh_buffer_desc_set = { };

    engine::Pipeline displace_pipeline = { };
};

void init_terrain_ray_tracing_info(
    TerrainRayTracingInfo* out_info, geometry::Terrain* terrain );

void add_terrain_rt_displace_pass( TerrainRayTracingInfo& info, engine::TaskList& task_list );

void add_terrain_rt_build_pass( TerrainRayTracingInfo& info, engine::TaskList& task_list );

}

#endif // RACECAR_RAY_TRACING
