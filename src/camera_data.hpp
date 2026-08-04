#pragma once

#include "context.hpp"
#include "engine/state.hpp"
#include "engine/ub_data.hpp"
#include "engine/uniform_buffer.hpp"
#include "gui.hpp"

#include <glm/glm.hpp>

namespace racecar {

struct CameraData {
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 position;
};

/*
 * Derives the view/projection matrices and eye position from engine.camera. Any input handling or
 * demo-driven camera motion must already have been applied.
 */
CameraData get_camera_data();

void update_camera_uniform_buffer( gui::Gui& gui,
    UniformBuffer<ub_data::Camera>& camera_buffer, const CameraData& camera_data );

}
