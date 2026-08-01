#include "scene_pass.hpp"

#include "../engine/draw_task.hpp"
#include "../engine/task_list.hpp"
#include "../vk/create.hpp"

namespace racecar {

namespace {

constexpr std::string_view SHADER_MODULE_PATH = "../shaders/deferred/prepass.spv";

}

void create_scene_gfx_pipeline(
    Context& ctx,
    engine::State& engine,
    engine::Pipeline* scene_pipeline,
    const geometry::scene::Mesh& scene_mesh,
    engine::DescriptorSet* uniform_desc_set,
    engine::DescriptorSet* material_desc_sets,
    engine::DescriptorSet* model_mat_desc_sets,
    engine::DescriptorSet* lut_sets,
    engine::DescriptorSet* sampler_desc_set
)
{
    try {
        size_t frame_index = engine.get_frame_index();
        *scene_pipeline = create_gfx_pipeline(
            engine,
            ctx.vulkan,
            engine::get_vertex_input_state_create_info( scene_mesh ),
            {
                uniform_desc_set->layouts[frame_index],
                material_desc_sets->layouts[frame_index],
                model_mat_desc_sets->layouts[frame_index],
                lut_sets->layouts[frame_index],
                sampler_desc_set->layouts[frame_index],
            },
            {
                VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, // POSITION
                VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, // NORMAL
                VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, // TANGENT
                VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, // UV
                VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, // ALBEDO
                VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, // PACKED DATA (metallic, roughness,
                                                         // clearcoat roughness, clearcoat
                                                         // weight)
                VkFormat::VK_FORMAT_R16G16_SFLOAT, // VELOCITY
            },
            VK_SAMPLE_COUNT_1_BIT,
            false,
            true,
            vk::create::shader_module( ctx.vulkan, SHADER_MODULE_PATH ),
            false
        );
    } catch ( const Exception& ex ) {
        log::error( "Failed to create graphics pipeline: {}", ex.what() );
        throw;
    }
}

engine::GfxTask create_prepass_gfx_task( const engine::State& engine, deferred::GBuffers& gbuffers )
{
    return {
        .clear_color = { { { 0.f, 0.f, 0.f, 0.f } } },
        .clear_depth = 1.f,
        .render_target_is_swapchain = false,
        .color_attachments = deferred::get_color_attachments( gbuffers ),
        .depth_image = deferred::get_depth_image( gbuffers ),
        .extent = engine.swapchain.extent,
    };
}

/*
 * Create a draw task for each prim and add it to the scene pass gfx task and the depth
 * ms gfx task, including all the necessary descriptor sets.
 */
void add_prim_draw_tasks(
    geometry::scene::Mesh& scene_mesh,
    const std::vector<const scene::Primitive*>& prims,
    ScenePassTarget scene_pass,
    DepthPassTarget depth_pass
)
{
    for ( const scene::Primitive* prim : prims ) {
        // Create a new draw resource descriptor for this primitive
        engine::DrawResourceDescriptor draw_descriptor = engine::DrawResourceDescriptor::from_mesh(
            scene_mesh.mesh_buffers.vertex_buffer.handle,
            scene_mesh.mesh_buffers.index_buffer.handle,
            static_cast<uint32_t>( scene_mesh.indices.size() ),
            *prim
        );

        // Give the material descriptor set to the draw task
        scene_pass.gfx_task.draw_tasks.push_back( {
                .draw_resource_descriptor = draw_descriptor,
                .descriptor_sets = {
                    &scene_pass.uniform_desc_set,
                    &scene_pass.material_desc_sets[static_cast<size_t>( prim->material_id )],
                    &scene_pass.model_mat_desc_sets[static_cast<size_t>( prim->node_id )],
                    &scene_pass.lut_sets,
                    &scene_pass.sampler_desc_set,
                },
                .pipeline = scene_pass.pipeline,
            } );
        depth_pass.gfx_task.draw_tasks.push_back(
            {
                .draw_resource_descriptor = draw_descriptor,
                .descriptor_sets = { &depth_pass.uniform_desc_set },
                .pipeline = depth_pass.pipeline,
            }
        );
    }
}

}
