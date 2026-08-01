#include "reflection_pass.hpp"

#include "../engine/draw_task.hpp"
#include "../engine/pipeline_barrier.hpp"
#include "../geometry/quad.hpp"
#include "../vk/create.hpp"

namespace racecar {

namespace {

constexpr std::string_view REFLECTION_PASS_SHADER_MODULE_PATH
    = "../shaders/reflections/reflections.spv";

}

void create_deferred_reflection_pipeline_barrier(
    engine::TaskList& task_list, deferred::GBuffers& gbuffers, engine::RWImage& reflection_data
)
{
    engine::add_pipeline_barrier(
        task_list,
        engine::PipelineBarrierDescriptor {
            .buffer_barriers = { },
            .image_barriers = {
                deferred::color_write_to_frag_read( gbuffers.GBuffer_Position ),
                deferred::color_write_to_frag_read( gbuffers.GBuffer_Velocity ),
                deferred::color_write_to_frag_read( gbuffers.GBuffer_Normal ),
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                                       .src_access = VK_ACCESS_2_NONE,
                                       .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                                       .dst_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                       .dst_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                       .dst_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       .image = reflection_data,
                                       .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR },
            } }
    );
}

void create_reflection_pass_resources(
    Context& ctx,
    engine::State& engine,
    ReflectionPassDescSets desc_sets,
    engine::RWImage* reflection_data,
    engine::Pipeline* reflection_pipeline,
    engine::DescriptorSet* reflection_buffer_desc_set,
    engine::GfxTask* reflection_gfx_task
)
{
    geometry::quad::Mesh& quad_mesh = geometry::quad::Mesh::get_instance();

    *reflection_data = engine::create_rwimage(
        ctx.vulkan,
        engine,
        VkExtent3D( engine.swapchain.extent.width, engine.swapchain.extent.height, 1 ),
        VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT,
        VkImageType::VK_IMAGE_TYPE_2D,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    );

    *reflection_pipeline = engine::create_gfx_pipeline(
        engine,
        ctx.vulkan,
        engine::get_vertex_input_state_create_info( quad_mesh ),
        { desc_sets.uniform_desc_set.layouts[0],
          desc_sets.sampler_desc_set.layouts[0],
          desc_sets.gbuffer_desc_set.layouts[0],
          desc_sets.car_tlas_desc_set.layouts[0],
          desc_sets.terrain_tlas_desc_set.layouts[0],
          desc_sets.car_desc_set.layouts[0],
          desc_sets.combined_textures_desc_set.layouts[0] },
        { VK_FORMAT_R16G16B16A16_SFLOAT },
        VK_SAMPLE_COUNT_1_BIT,
        false,
        false,
        vk::create::shader_module( ctx.vulkan, REFLECTION_PASS_SHADER_MODULE_PATH ),
        false
    );

    engine::DrawResourceDescriptor reflection_prepass_desc {
        .vertex_buffers = { quad_mesh.mesh_buffers.vertex_buffer.handle },
        .index_buffer = quad_mesh.mesh_buffers.index_buffer.handle,
        .vertex_buffer_offsets = { 0 },
        .index_count = uint32_t( quad_mesh.indices.size() ),
    };

    engine::DrawTask reflection_prepass_task { .draw_resource_descriptor = reflection_prepass_desc,
                                               .descriptor_sets
                                               = { &desc_sets.uniform_desc_set,
                                                   &desc_sets.sampler_desc_set,
                                                   &desc_sets.gbuffer_desc_set,
                                                   &desc_sets.car_tlas_desc_set,
                                                   &desc_sets.terrain_tlas_desc_set,
                                                   &desc_sets.car_desc_set,
                                                   &desc_sets.combined_textures_desc_set },
                                               .pipeline = *reflection_pipeline };

    *reflection_buffer_desc_set = engine::generate_descriptor_set(
        ctx.vulkan,
        engine,
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
    );

    engine::update_descriptor_set_rwimage(
        ctx.vulkan,
        engine,
        *reflection_buffer_desc_set,
        *reflection_data,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        0
    );

    *reflection_gfx_task = { .clear_color = { { { 0.0f, 0.0f, 0.0f, 0.0f } } },
                             .color_attachments = { *reflection_data },
                             .extent = engine.swapchain.extent };

    reflection_gfx_task->draw_tasks.push_back( reflection_prepass_task );
}

}
