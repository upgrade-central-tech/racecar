#include "gui_uniforms.hpp"

namespace racecar {

void update_debug_uniform_buffer(
    Context& ctx,
    engine::State& engine,
    gui::Gui& gui,
    atmosphere::Atmosphere& atms,
    UniformBuffer<ub_data::Debug>& debug_buffer
)
{
    ub_data::Atmosphere atms_ub = atms.uniform_buffer.get_data();

    ub_data::Debug debug_ub = {
        .color = gui.debug.color,
        .packed_data0 = glm::vec4(
            gui.debug.roughness,
            gui.debug.metallic,
            gui.debug.clearcoat_roughness,
            gui.debug.clearcoat_weight
        ),
        .sun_direction = glm::vec4( atms_ub.sun_direction, 1.0f ),

        .enable_albedo_map = gui.debug.enable_albedo_map,
        .enable_normal_map = gui.debug.enable_normal_map,
        .enable_roughness_metal_map = gui.debug.enable_roughness_metal_map,
        .normals_only = gui.debug.normals_only,
        .albedo_only = gui.debug.albedo_only,
        .roughness_metal_only = gui.debug.roughness_metal_only,

        .ray_traced_shadows = gui.debug.ray_traced_shadows,
    };

    debug_buffer.set_data( debug_ub );
    debug_buffer.update( ctx.vulkan, engine.get_frame_index() );
}

void update_material_uniform_buffers(
    Context& ctx,
    engine::State& engine,
    gui::Gui& gui,
    std::vector<UniformBuffer<ub_data::Material>>& material_uniform_buffers,
    size_t num_materials
)
{
    gui.debug.current_editing_material
        = glm::clamp( gui.debug.current_editing_material, 0, int( num_materials ) );
    int mat_idx = gui.debug.current_editing_material;
    auto mat_data = material_uniform_buffers[size_t( mat_idx )].get_data();
    if ( gui.debug.load_material_into_gui ) {
        gui.debug.color = mat_data.base_color;
        gui.debug.roughness = mat_data.roughness;
        gui.debug.metallic = mat_data.metallic;
        gui.debug.clearcoat_weight = mat_data.clearcoat;
        gui.debug.clearcoat_roughness = mat_data.clearcoat_roughness;
        gui.debug.load_material_into_gui = false;
    }
    mat_data.base_color = gui.debug.color;
    mat_data.roughness = gui.debug.roughness;
    mat_data.metallic = gui.debug.metallic;
    mat_data.clearcoat = gui.debug.clearcoat_weight;
    mat_data.clearcoat_roughness = gui.debug.clearcoat_roughness;

    mat_data.glintiness = gui.debug.glintiness;
    mat_data.glint_log_density = gui.debug.glint_log_density;
    mat_data.glint_roughness = gui.debug.glint_roughness;
    mat_data.glint_randomness = gui.debug.glint_randomness;

    material_uniform_buffers[size_t( mat_idx )].set_data( mat_data );
    material_uniform_buffers[size_t( mat_idx )].update( ctx.vulkan, engine.get_frame_index() );
}

}
