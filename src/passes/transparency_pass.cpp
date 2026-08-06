#include "transparency_pass.hpp"

#include "vk/create.hpp"

namespace racecar {

constexpr std::string_view REFLECTION_PASS_SHADER_MODULE_PATH
    = "../shaders/transparency/transparency.spv";

void create_transparency_pass_resources(
    TransparencyPass* transparency_pass,
    const geometry::scene::Mesh& scene_mesh,
    engine::DescriptorSet* uniform_desc_set,
    std::vector<engine::DescriptorSet>* model_mat_desc_sets
)
{
    const engine::State& engine = engine::State::GetConst();
    size_t frame_index = engine.get_frame_index();

    transparency_pass->transparency_pipeline = engine::create_gfx_pipeline(
        engine::get_vertex_input_state_create_info( scene_mesh ),
        { uniform_desc_set->layouts[frame_index], (*model_mat_desc_sets)[0].layouts[frame_index] },
        { VK_FORMAT_R16G16B16A16_SFLOAT },
        VK_SAMPLE_COUNT_1_BIT,
        true,
        true,
        vk::create::shader_module( REFLECTION_PASS_SHADER_MODULE_PATH ),
        false
    );

    transparency_pass->uniform_desc_set = uniform_desc_set;
    transparency_pass->model_mat_desc_sets = model_mat_desc_sets;
}

void execute_transparency_pass(
    TransparencyPass* transparency_pass,
    geometry::scene::Mesh& scene_mesh,
    std::vector<const scene::Primitive*>& transparent_prims,
    engine::RWImage* screen_color,
    engine::RWImage* gbuffer_depth_image,
    engine::TaskList& task_list
)
{
    const engine::State& engine = engine::State::GetConst();

    // transparency pass depth sort cpu task

    transparency_pass->transparency_gfx_task = engine::GfxTask
    {
        .clear_color = std::nullopt, .clear_depth = std::nullopt,
        .color_attachments = { screen_color }, .depth_image = gbuffer_depth_image,
        .extent = engine.swapchain.extent
    };

    // loop and add draw tasks
    for (const scene::Primitive* prim : transparent_prims) {
        engine::DrawResourceDescriptor draw_descriptor = engine::DrawResourceDescriptor::from_mesh(
            scene_mesh.mesh_buffers.vertex_buffer.handle,
            scene_mesh.mesh_buffers.index_buffer.handle,
            static_cast<uint32_t>( scene_mesh.indices.size() ),
            *prim
        );

        transparency_pass->transparency_gfx_task.draw_tasks.push_back({
            .draw_resource_descriptor = draw_descriptor,
            .descriptor_sets = {
                transparency_pass->uniform_desc_set,
                &(*transparency_pass->model_mat_desc_sets)[static_cast<size_t>( prim->node_id )]
            },
            .pipeline = transparency_pass->transparency_pipeline
        });
    }

    engine::add_gfx_task( task_list, transparency_pass->transparency_gfx_task );
}

void create_lighting_transparency_pipeline_barrier(
    engine::RWImage& screen_color,
    engine::RWImage& gbuffer_depth,
    engine::TaskList& task_list
)
{
    engine::add_pipeline_barrier(
        task_list,
        engine::PipelineBarrierDescriptor {
            .buffer_barriers = { },
            .image_barriers = {
                engine::ImageBarrier {
                    .src_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .src_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .dst_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .image = &screen_color,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
                },
                engine::ImageBarrier {
                    .src_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .src_access = VK_ACCESS_2_SHADER_READ_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                    .dst_access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    .image = &gbuffer_depth,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_DEPTH,
                },
            } }
    );
}

}
