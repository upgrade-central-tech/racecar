#include "car_rt.hpp"

#include "../engine/descriptors.hpp"
#include "../engine/images.hpp"
#include "../vk/create.hpp"

namespace racecar {

void alloc_blases(
    Context& ctx,
    engine::State& engine,
    const std::vector<const scene::Primitive*>& prims,
    const geometry::scene::Mesh& scene_mesh,
    ub_data::BLASOffsets* ret_blas_offsets
)
{
    int blas_count = 0;
    ub_data::BLASOffsets blas_offsets { };
    for ( const scene::Primitive* prim : prims ) {
        uint32_t max_idx = 0;
        for ( size_t x = 0; x < prim->ind_count; x++ ) {
            size_t offset_x = x + size_t( prim->ind_offset );
            uint32_t idx = scene_mesh.indices[offset_x];
            max_idx = glm::max( max_idx, idx );
        }
        vk::rt::AccelerationStructure as;
        vk::rt::alloc_blas(
            ctx.vulkan.device,
            ctx.vulkan.allocator,
            as,
            ctx.vulkan.ray_tracing_properties,
            { .vertex_buffer = scene_mesh.mesh_buffers.vertex_buffer.handle,
              .index_buffer = scene_mesh.mesh_buffers.index_buffer.handle,
              .max_vertex = uint32_t( max_idx ) - 1,
              .index_count = uint32_t( prim == nullptr ? 0 : prim->ind_count ),
              .vertex_offset = uint32_t( prim == nullptr ? 0 : prim->vertex_offset ),
              .index_offset = uint32_t( prim == nullptr ? 0 : prim->ind_offset ),
              .vertex_stride = sizeof( geometry::scene::Vertex ) },
            ctx.vulkan.destructor_stack
        );
        engine.blas.push_back( as );

        blas_offsets.vertex_buffer_offset[blas_count]
            = uint32_t( prim == nullptr ? 0 : prim->vertex_offset );
        blas_offsets.index_buffer_offset[blas_count]
            = uint32_t( prim == nullptr ? 0 : prim->ind_offset );

        blas_count++;
    }

    *ret_blas_offsets = blas_offsets;
}

void create_objects(
    engine::State& engine, std::vector<glm::mat4>& transforms, std::vector<vk::rt::Object>& objects
)
{
    for ( size_t i = 0; i < engine.blas.size(); i++ ) {
        auto& blas = engine.blas[i];
        objects.push_back( vk::rt::Object { .blas = &blas, .transform = transforms[i] } );
    }
}

engine::DescriptorSet
create_accel_structure_desc_set( vk::Common& vulkan, const engine::State& engine )
{
    return engine::generate_descriptor_set(
        vulkan,
        engine,
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR },
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
    );
}

void alloc_car_tlas(
    vk::Common& vulkan, engine::State& engine, const std::vector<vk::rt::Object>& objects
)
{
    vk::rt::alloc_tlas(
        vulkan.device,
        vulkan.allocator,
        engine.tlas,
        vulkan.ray_tracing_properties,
        objects,
        vulkan.destructor_stack
    );
}

void build_car_blases( engine::State& engine, VkCommandBuffer& precompute_cmdbuf )
{
    for ( vk::rt::AccelerationStructure& blas : engine.blas ) {
        vk::rt::build_blas( precompute_cmdbuf, blas );
    }
}

void build_car_tlas( engine::State& engine, VkCommandBuffer& precompute_cmdbuf )
{
    vk::rt::build_tlas( precompute_cmdbuf, engine.tlas );
}

vk::mem::AllocatedBuffer create_padded_vertex_data_buffer(
    Context& ctx, engine::State& engine, geometry::scene::Mesh& scene_mesh
)
{
    std::vector<ub_data::PaddedVertex> padded_vertex_data { };
    for ( geometry::scene::Vertex& v : scene_mesh.vertices ) {
        padded_vertex_data.push_back(
            { .position = v.position, .normal = v.normal, .tangent = v.tangent, .uv = v.uv }
        );
    }

    vk::mem::AllocatedBuffer padded_vertex_data_buffer = vk::mem::create_buffer(
        ctx.vulkan,
        padded_vertex_data.size() * sizeof( ub_data::PaddedVertex ),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    vk::mem::AllocatedBuffer staging = vk::mem::create_buffer(
        ctx.vulkan,
        padded_vertex_data.size() * sizeof( ub_data::PaddedVertex ),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY
    );

    void* data = staging.info.pMappedData;
    std::memcpy(
        data,
        padded_vertex_data.data(),
        padded_vertex_data.size() * sizeof( ub_data::PaddedVertex )
    );

    engine::immediate_submit( ctx.vulkan, engine.immediate_submit, [&]( VkCommandBuffer cmd_buf ) {
        VkBufferCopy vertex_buf_copy = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = padded_vertex_data.size() * sizeof( ub_data::PaddedVertex ),
        };

        vkCmdCopyBuffer(
            cmd_buf,
            staging.handle,
            padded_vertex_data_buffer.handle,
            1,
            &vertex_buf_copy
        );
    } );

    return padded_vertex_data_buffer;
}

engine::DescriptorSet create_car_desc_set(
    Context& ctx,
    engine::State& engine,
    geometry::scene::Mesh& scene_mesh,
    vk::mem::AllocatedBuffer& padded_vertex_data_buffer,
    UniformBuffer<ub_data::BLASOffsets>& offset_data,
    vk::mem::AllocatedImage& lut_brdf,
    atmosphere::AtmosphereBaker& atms_baker,
    UniformBuffer<ub_data::RTTextureUniform>& rt_texture_uniform_data
)
{
    engine::DescriptorSet car_descriptor_set = engine::generate_descriptor_set(
        ctx.vulkan,
        engine,
        {
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // vertex_data
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // index_data
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // blas_offsets
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // rt_texture_uniform,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // BRDF_LUT
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // octahedral_sky_mips
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // octahedral_sky_irradiance
        },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    );

    engine::update_descriptor_set_const_storage_buffer(
        ctx.vulkan,
        engine,
        car_descriptor_set,
        padded_vertex_data_buffer,
        0
    );

    engine::update_descriptor_set_const_storage_buffer(
        ctx.vulkan,
        engine,
        car_descriptor_set,
        scene_mesh.mesh_buffers.index_buffer,
        1
    );

    engine::update_descriptor_set_uniform( ctx.vulkan, engine, car_descriptor_set, offset_data, 2 );

    engine::update_descriptor_set_image( ctx.vulkan, engine, car_descriptor_set, lut_brdf, 4 );
    engine::update_descriptor_set_rwimage(
        ctx.vulkan,
        engine,
        car_descriptor_set,
        atms_baker.octahedral_sky_mips,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        5
    );
    engine::update_descriptor_set_rwimage(
        ctx.vulkan,
        engine,
        car_descriptor_set,
        atms_baker.octahedral_sky_irradiance,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        6
    );

    engine::update_descriptor_set_uniform(
        ctx.vulkan,
        engine,
        car_descriptor_set,
        rt_texture_uniform_data,
        3
    );

    return car_descriptor_set;
}

engine::DescriptorSet create_combined_textures_desc_set(
    Context& ctx,
    engine::State& engine,
    std::vector<vk::mem::AllocatedImage>& albedo_textures,
    std::vector<vk::mem::AllocatedImage>& metallic_roughness_textures
)
{
    engine::DescriptorSet combined_textures_desc_set = engine::generate_array_descriptor_set(
        ctx.vulkan,
        engine,
        {
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // albedo_textures array
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // metallic_roughness_textures array
        },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        uint32_t( albedo_textures.size() )
    );

    engine::update_descriptor_set_image_array(
        ctx.vulkan,
        engine,
        combined_textures_desc_set,
        albedo_textures,
        0
    );
    engine::update_descriptor_set_image_array(
        ctx.vulkan,
        engine,
        combined_textures_desc_set,
        metallic_roughness_textures,
        1
    );

    return combined_textures_desc_set;
}

void update_rt_uniform_buffers(
    Context& ctx,
    engine::State& engine,
    UniformBuffer<ub_data::BLASOffsets>& offset_data,
    UniformBuffer<ub_data::RTTextureUniform>& rt_texture_uniform_data
)
{
    offset_data.update( ctx.vulkan, engine.get_frame_index() );
    rt_texture_uniform_data.update( ctx.vulkan, engine.get_frame_index() );
}

}
