#pragma once

#include "../context.hpp"
#include "../engine/state.hpp"
#include "../engine/ub_data.hpp"
#include "../engine/uniform_buffer.hpp"
#include "../gui.hpp"
#include "../orbit_camera.hpp"
#include "../scene/scene.hpp"
#include "../volumetrics.hpp"

#include <vector>

namespace racecar {

/*
 * Demo-driven camera motion: locks the camera onto the car when enabled, then applies the
 * bumpiness bob. Must run after camera::process_input, whose result it overrides.
 */
void apply_demo_camera_motion( camera::OrbitCamera& camera, const gui::Gui& gui,
    const scene::Scene& scene,
    const std::vector<UniformBuffer<ub_data::ModelMat>>& model_mat_uniform_buffers,
    const volumetric::Volumetric& volumetric );

void update_car_transform( gui::Gui& gui, scene::Scene& scene,
    std::vector<UniformBuffer<ub_data::ModelMat>>& model_mat_uniform_buffers,
    const volumetric::Volumetric& volumetric, std::vector<bool>& discovered );

void update_wheel_transforms(
    scene::Scene& scene, std::vector<UniformBuffer<ub_data::ModelMat>>& model_mat_uniform_buffers,
    std::vector<bool>& discovered );

}
