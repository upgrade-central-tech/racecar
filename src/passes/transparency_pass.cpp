#include "transparency_pass.hpp"

#include "vk/create.hpp"

#include <algorithm>
#include <limits>

namespace racecar {

namespace {

constexpr std::string_view TRANSPARENCY_SHADER_MODULE_PATH
    = "../shaders/transparency/transparency.spv";

glm::vec3
compute_primitive_centroid( const geometry::scene::Mesh& scene_mesh, const scene::Primitive& prim )
{
    if ( prim.ind_count == 0 ) {
        return glm::vec3( 0.f );
    }

    glm::vec3 aabb_min( std::numeric_limits<float>::max() );
    glm::vec3 aabb_max( std::numeric_limits<float>::lowest() );

    for ( size_t i = 0; i < prim.ind_count; ++i ) {
        const uint32_t idx = scene_mesh.indices[size_t( prim.ind_offset ) + i];
        const glm::vec3& position
            = scene_mesh.vertices[size_t( prim.vertex_offset ) + size_t( idx )].position;

        aabb_min = glm::min( aabb_min, position );
        aabb_max = glm::max( aabb_max, position );
    }

    return 0.5f * ( aabb_min + aabb_max );
}

}

void create_transparency_pass_resources(
    TransparencyPass* transparency_pass,
    const geometry::scene::Mesh& scene_mesh,
    const std::vector<const scene::Primitive*>& transparent_prims,
    engine::DescriptorSet* uniform_desc_set,
    std::vector<engine::DescriptorSet>* model_mat_desc_sets
)
{
    const engine::State& engine = engine::State::GetConst();
    size_t frame_index = engine.get_frame_index();

    transparency_pass->transparency_pipeline = engine::create_gfx_pipeline(
        engine::get_vertex_input_state_create_info( scene_mesh ),
        { uniform_desc_set->layouts[frame_index],
          ( *model_mat_desc_sets )[0].layouts[frame_index] },
        { VK_FORMAT_R16G16B16A16_SFLOAT },
        VK_SAMPLE_COUNT_1_BIT,
        true,
        true,
        vk::create::shader_module( TRANSPARENCY_SHADER_MODULE_PATH ),
        false
    );

    transparency_pass->uniform_desc_set = uniform_desc_set;
    transparency_pass->model_mat_desc_sets = model_mat_desc_sets;

    transparency_pass->prim_info.clear();
    transparency_pass->prim_info.reserve( transparent_prims.size() );

    for ( const scene::Primitive* prim : transparent_prims ) {
        transparency_pass->prim_info.push_back(
            TransparentPrimInfo {
                .index_offset = prim->ind_offset,
                .node_id = prim->node_id,
                .centroid = compute_primitive_centroid( scene_mesh, *prim ),
            }
        );
    }
}

void execute_transparency_pass(
    TransparencyPass* transparency_pass,
    geometry::scene::Mesh& scene_mesh,
    const std::vector<const scene::Primitive*>& transparent_prims,
    engine::RWImage* screen_color,
    engine::RWImage* gbuffer_depth_image,
    const UniformBuffer<ub_data::Camera>& camera_buffer,
    const std::vector<UniformBuffer<ub_data::ModelMat>>& model_mat_uniform_buffers,
    engine::TaskList& task_list
)
{
    const engine::State& engine = engine::State::GetConst();

    engine::GfxTask transparency_gfx_task = {
        .clear_color = std::nullopt,
        .clear_depth = std::nullopt,
        .color_attachments = { screen_color },
        .depth_image = gbuffer_depth_image,
        .extent = engine.swapchain.extent,
    };

    for ( const scene::Primitive* prim : transparent_prims ) {
        engine::DrawResourceDescriptor draw_descriptor = engine::DrawResourceDescriptor::from_mesh(
            scene_mesh.mesh_buffers.vertex_buffer.handle,
            scene_mesh.mesh_buffers.index_buffer.handle,
            static_cast<uint32_t>( scene_mesh.indices.size() ),
            *prim
        );

        transparency_gfx_task.draw_tasks.push_back({
            .draw_resource_descriptor = draw_descriptor,
            .descriptor_sets = {
                transparency_pass->uniform_desc_set,
                &( *transparency_pass->model_mat_desc_sets )[static_cast<size_t>( prim->node_id )],
            },
            .pipeline = transparency_pass->transparency_pipeline,
        });
    }

    const size_t gfx_idx = task_list.gfx_tasks.size();

    engine::add_cpu_task(
        task_list,
        [transparency_pass, gfx_idx, &task_list, &camera_buffer, &model_mat_uniform_buffers]() {
            std::vector<engine::DrawTask>& draws = task_list.gfx_tasks[gfx_idx].draw_tasks;

            const glm::mat4 view = camera_buffer.get_data().view_mat;

            const auto view_z = [&]( const engine::DrawTask& draw ) -> float {
                const int32_t key = draw.draw_resource_descriptor.index_offset;

                const auto info = std::find_if(
                    transparency_pass->prim_info.begin(),
                    transparency_pass->prim_info.end(),
                    [key]( const TransparentPrimInfo& i ) { return i.index_offset == key; }
                );

                if ( info == transparency_pass->prim_info.end() ) {
                    return 0.f;
                }

                const glm::mat4 model
                    = model_mat_uniform_buffers[static_cast<size_t>( info->node_id )]
                          .get_data()
                          .model_mat;

                return ( view * model * glm::vec4( info->centroid, 1.f ) ).z;
            };

            std::sort(
                draws.begin(),
                draws.end(),
                [&]( const engine::DrawTask& a, const engine::DrawTask& b ) {
                    const float za = view_z( a );
                    const float zb = view_z( b );

                    if ( za != zb ) {
                        return za < zb;
                    }

                    return a.draw_resource_descriptor.index_offset
                        < b.draw_resource_descriptor.index_offset;
                }
            );
        }
    );

    engine::add_gfx_task( task_list, std::move( transparency_gfx_task ) );
}

void create_lighting_transparency_pipeline_barrier(
    engine::RWImage& screen_color, engine::RWImage& gbuffer_depth, engine::TaskList& task_list
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
                    .dst_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
                        | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .image = &screen_color,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
                },
                engine::ImageBarrier {
                    .src_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .src_access = VK_ACCESS_2_SHADER_READ_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                    .dst_access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                        | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    .image = &gbuffer_depth,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_DEPTH,
                },
            } }
    );
}

}
