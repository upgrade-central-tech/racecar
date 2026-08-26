#if RACECAR_RAY_TRACING

#include "terrain_rt.hpp"

#include "../engine/state.hpp"
#include "../vk/create.hpp"

namespace racecar {

namespace {

const std::filesystem::path TERRAIN_RT_DISPLACE_SHADER_MODULE_PATH
    = "../shaders/terrain/terrain_rt_displace.spv";

constexpr uint32_t DISPLACE_GROUP_SIZE = 8;

void create_mesh_buffers( TerrainRayTracingInfo* info )
{
    const engine::State& engine = engine::State::GetConst();

    info->dynamic_terrain_mesh_buffer.clear();
    info->dynamic_terrain_mesh_buffer.reserve( engine.frame_overlap );

    for ( uint32_t i = 0; i < engine.frame_overlap; ++i ) {
        info->dynamic_terrain_mesh_buffer.push_back(
            vk::mem::create_buffer(
                sizeof( TerrainRTVertex ) * TERRAIN_RT_VERTEX_COUNT,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                    | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                VMA_MEMORY_USAGE_GPU_ONLY
            )
        );
    }
}

void create_grid_uniform( TerrainRayTracingInfo* info )
{
    const engine::State& engine = engine::State::GetConst();

    info->grid_uniform = create_uniform_buffer<TerrainRTGrid>( { }, engine.frame_overlap );

    info->grid_uniform.set_data(
        TerrainRTGrid {
            .world_min = glm::vec2( -TERRAIN_RT_WORLD_HALF_WIDTH, -TERRAIN_RT_WORLD_HALF_WIDTH ),
            .world_size
            = glm::vec2( 2.0f * TERRAIN_RT_WORLD_HALF_WIDTH, 2.0f * TERRAIN_RT_WORLD_HALF_WIDTH ),
            .vertex_count = glm::uvec2( TERRAIN_RT_VERTS_PER_SIDE, TERRAIN_RT_VERTS_PER_SIDE ),
            ._pad = glm::uvec2( 0 ),
        }
    );
    info->grid_uniform.update_all();
}

void create_descriptor_sets( TerrainRayTracingInfo* info )
{
    const vk::Common& vulkan = vk::Common::GetConst();

    info->uniform_desc_set = engine::generate_descriptor_set(
        {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // terrain data
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // terrain grid
        },
        VK_SHADER_STAGE_COMPUTE_BIT
    );

    info->texture_desc_set = engine::generate_descriptor_set(
        {
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // layer_test_mask
        },
        VK_SHADER_STAGE_COMPUTE_BIT
    );

    info->sampler_desc_set = engine::generate_descriptor_set(
        {
            VK_DESCRIPTOR_TYPE_SAMPLER, // linear_sampler
        },
        VK_SHADER_STAGE_COMPUTE_BIT
    );

    info->mesh_buffer_desc_set = engine::generate_descriptor_set(
        {
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // out_vertices
        },
        VK_SHADER_STAGE_COMPUTE_BIT
    );

    engine::update_descriptor_set_uniform( info->uniform_desc_set, *info->terrain_uniform, 0 );
    engine::update_descriptor_set_uniform( info->uniform_desc_set, info->grid_uniform, 1 );

    engine::update_descriptor_set_image( info->texture_desc_set, *info->layer_mask, 0 );

    engine::update_descriptor_set_sampler(
        info->sampler_desc_set,
        vulkan.global_samplers.linear_sampler,
        0
    );

    engine::update_descriptor_set_storage_buffer_per_frame(
        info->mesh_buffer_desc_set,
        info->dynamic_terrain_mesh_buffer,
        0
    );
}

void create_index_buffer( TerrainRayTracingInfo* info )
{
    const engine::State& engine = engine::State::GetConst();

    std::vector<uint32_t> indices;
    indices.reserve( TERRAIN_RT_INDEX_COUNT );

    for ( uint32_t z = 0; z < TERRAIN_RT_QUADS_PER_SIDE; ++z ) {
        for ( uint32_t x = 0; x < TERRAIN_RT_QUADS_PER_SIDE; ++x ) {
            const uint32_t top_left = z * TERRAIN_RT_VERTS_PER_SIDE + x;
            const uint32_t top_right = top_left + 1;
            const uint32_t bottom_left = ( z + 1 ) * TERRAIN_RT_VERTS_PER_SIDE + x;
            const uint32_t bottom_right = bottom_left + 1;

            indices.push_back( top_left );
            indices.push_back( bottom_left );
            indices.push_back( top_right );

            indices.push_back( bottom_left );
            indices.push_back( bottom_right );
            indices.push_back( top_right );
        }
    }

    const size_t byte_size = indices.size() * sizeof( uint32_t );

    info->terrain_index_buffer = vk::mem::create_buffer(
        byte_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    vk::mem::AllocatedBuffer staging = vk::mem::create_buffer(
        byte_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY
    );

    std::memcpy( staging.info.pMappedData, indices.data(), byte_size );

    engine::immediate_submit( engine.immediate_submit, [&]( VkCommandBuffer cmd_buf ) {
        VkBufferCopy copy = { .srcOffset = 0, .dstOffset = 0, .size = byte_size };
        vkCmdCopyBuffer( cmd_buf, staging.handle, info->terrain_index_buffer.handle, 1, &copy );
    } );
}

void create_shading_desc_set(
    TerrainRayTracingInfo* info, const std::vector<VkAccelerationStructureKHR>& tlas_handles
)
{
    const vk::Common& vulkan = vk::Common::GetConst();
    geometry::Terrain& terrain = *info->terrain;

    info->shading_desc_set = engine::generate_descriptor_set(
        {
            VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, // terrain TLAS
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // terrain data
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // layer mask
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // grass albedo + roughness
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // grass normal + ao
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // asphalt albedo + roughness
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // asphalt normal + ao
            VK_DESCRIPTOR_TYPE_SAMPLER, // linear sampler
        },
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR
    );

    engine::update_descriptor_set_acceleration_structure_per_frame(
        info->shading_desc_set,
        tlas_handles,
        0
    );

    engine::update_descriptor_set_uniform( info->shading_desc_set, *info->terrain_uniform, 1 );

    engine::update_descriptor_set_image( info->shading_desc_set, terrain.test_layer_mask, 2 );
    engine::update_descriptor_set_image(
        info->shading_desc_set,
        terrain.grass_albedo_roughness,
        3
    );
    engine::update_descriptor_set_image( info->shading_desc_set, terrain.grass_normal_ao, 4 );
    engine::update_descriptor_set_image(
        info->shading_desc_set,
        terrain.asphalt_albedo_roughness,
        5
    );
    engine::update_descriptor_set_image( info->shading_desc_set, terrain.asphalt_normal_ao, 6 );

    engine::update_descriptor_set_sampler(
        info->shading_desc_set,
        vulkan.global_samplers.linear_sampler,
        7
    );
}

void create_acceleration_structures( TerrainRayTracingInfo* info )
{
    vk::Common& vulkan = vk::Common::GetMut();
    const engine::State& engine = engine::State::GetConst();

    info->dynamic_terrain_blas.clear();
    info->dynamic_terrain_blas.resize( engine.frame_overlap );

    info->tlas.clear();
    info->tlas.resize( engine.frame_overlap );

    for ( uint32_t i = 0; i < engine.frame_overlap; ++i ) {
        vk::rt::alloc_blas(
            vulkan.device,
            vulkan.allocator,
            info->dynamic_terrain_blas[i],
            vulkan.ray_tracing_properties,
            { .vertex_buffer = info->dynamic_terrain_mesh_buffer[i].handle,
              .index_buffer = info->terrain_index_buffer.handle,
              .max_vertex = TERRAIN_RT_VERTEX_COUNT - 1,
              .index_count = TERRAIN_RT_INDEX_COUNT,
              .vertex_offset = 0,
              .index_offset = 0,
              .vertex_stride = sizeof( TerrainRTVertex ) },
            vulkan.destructor_stack
        );

        vk::rt::alloc_tlas(
            vulkan.device,
            vulkan.allocator,
            info->tlas[i],
            vulkan.ray_tracing_properties,
            { vk::rt::Object { .blas = &info->dynamic_terrain_blas[i],
                               .transform = glm::identity<glm::mat4>() } },
            vulkan.destructor_stack
        );
    }

    info->tlas_desc_set = engine::generate_descriptor_set(
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR },
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
    );

    std::vector<VkAccelerationStructureKHR> tlas_handles;
    tlas_handles.reserve( info->tlas.size() );
    for ( const vk::rt::AccelerationStructure& tlas : info->tlas ) {
        tlas_handles.push_back( tlas.handle );
    }

    engine::update_descriptor_set_acceleration_structure_per_frame(
        info->tlas_desc_set,
        tlas_handles,
        0
    );

    create_shading_desc_set( info, tlas_handles );
}

}

void init_terrain_ray_tracing_info( TerrainRayTracingInfo* out_info, geometry::Terrain* terrain )
{
    *out_info = TerrainRayTracingInfo { .terrain = terrain,
                                        .terrain_uniform = &terrain->terrain_uniform,
                                        .layer_mask = &terrain->test_layer_mask };

    create_mesh_buffers( out_info );
    create_grid_uniform( out_info );
    create_descriptor_sets( out_info );

    out_info->displace_pipeline = engine::create_compute_pipeline(
        {
            out_info->uniform_desc_set.layouts[0],
            out_info->texture_desc_set.layouts[0],
            out_info->sampler_desc_set.layouts[0],
            out_info->mesh_buffer_desc_set.layouts[0],
        },
        vk::create::shader_module( TERRAIN_RT_DISPLACE_SHADER_MODULE_PATH ),
        "cs_terrain_rt_displace"
    );

    create_index_buffer( out_info );
    create_acceleration_structures( out_info );
}

void add_terrain_rt_displace_pass( TerrainRayTracingInfo& info, engine::TaskList& task_list )
{
    const int32_t groups_per_side = static_cast<int32_t>(
        ( TERRAIN_RT_VERTS_PER_SIDE + DISPLACE_GROUP_SIZE - 1 ) / DISPLACE_GROUP_SIZE
    );

    engine::ComputeTask displace_task = {
        info.displace_pipeline,
        {
            &info.uniform_desc_set,
            &info.texture_desc_set,
            &info.sampler_desc_set,
            &info.mesh_buffer_desc_set,
        },
        glm::ivec3( groups_per_side, groups_per_side, 1 ),
    };

    engine::add_cs_task( task_list, displace_task );
}

void add_terrain_rt_build_pass( TerrainRayTracingInfo& info, engine::TaskList& task_list )
{
    engine::add_pipeline_barrier(
        task_list,
        engine::PipelineBarrierDescriptor {
            .buffer_barriers = { engine::BufferBarrier {
                .buffer = info.dynamic_terrain_mesh_buffer,
                .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .src_access = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dst_stage = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
            } },
            .image_barriers = { },
        }
    );

    engine::add_gpu_task( task_list, [&info]( VkCommandBuffer cmd_buf ) {
        const engine::State& engine = engine::State::GetConst();
        const size_t frame = engine.get_frame_index();

        vk::rt::build_blas( cmd_buf, info.dynamic_terrain_blas[frame] );

        // lowk unnecessary for now
        vk::rt::build_tlas( cmd_buf, info.tlas[frame] );
    } );
}

}

#endif // RACECAR_RAY_TRACING
