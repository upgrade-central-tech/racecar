#pragma once

#include <SDL3/SDL_events.h>
#include <glm/glm.hpp>

namespace racecar::scene::camera {

struct Camera {
    glm::vec3 eye;
    glm::vec3 velocity;

    /// Optional, for DOF and centering camera on the car.
    glm::vec3 look_at;

    glm::mat4 view_mat;
    glm::vec3 forward;
    glm::vec3 up;
    glm::vec3 right;

    /// Not loaded from GLTF.
    double fov_y = 0.0;
    /// In radians.
    double aspect_ratio = 0.0;
    double near_plane = 0.0;
    double far_plane = 0.0;
};

glm::mat4 get_view_matrix( Camera& cam );
glm::mat4 get_proj_matrix( Camera& cam );
glm::mat4 get_view_proj_matrix( Camera& cam );

void process_sdl_event( SDL_Event& e, Camera& cam );

}  // namespace racecar::scene::camera
