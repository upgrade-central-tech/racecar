#pragma once

#include <SDL3/SDL_events.h>
#include <glm/glm.hpp>

namespace racecar::camera {

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
    float fov_y = 0.f;

    /// In radians.
    float aspect_ratio = 0.f;
    float near_plane = 0.f;
    float far_plane = 0.f;
};

glm::mat4 get_view_matrix( Camera& cam );
glm::mat4 get_proj_matrix( Camera& cam );
glm::mat4 get_view_proj_matrix( Camera& cam );

void process_event( const SDL_Event* event, Camera& cam );

}  // namespace racecar::camera
