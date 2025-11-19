#pragma once

#include "context.hpp"

#include <SDL3/SDL_events.h>
#include <glm/glm.hpp>

namespace racecar::camera {

/// Implements an orbit camera. Based on
/// https://www.mbsoftworks.sk/tutorials/opengl4/026-camera-pt3-orbit-camera.
struct OrbitCamera {
    glm::vec3 center = {}; ///< Also known as the center of the sphere.
    float radius = 0.f;

    float azimuth = 0.f; ///< Stored in radians.
    float zenith = 0.f; ///< Stored in radians.

    glm::vec3 up = {};

    float fov_y = 0.f;
    float aspect_ratio = 0.f;
    float near_plane = 0.f;
    float far_plane = 0.f;
};

/// Keeps azimuth in the range [0, 2π].
void rotate_azimuth( OrbitCamera& cam, float radians );

/// Keeps polar in the range [-π/2, π/2].
void rotate_polar( OrbitCamera& cam, float radians );

void move_along_view( OrbitCamera& cam, float delta );
void move_horizontal( OrbitCamera& cam, float delta );
void move_vertical( OrbitCamera& cam, float delta );
void zoom( OrbitCamera& cam, float delta );

/// Converts spherical coordinates to eye position and constructs the view matrix from it.
glm::mat4 calculate_view_matrix( const OrbitCamera& cam );
glm::mat4 calculate_proj_matrix( const OrbitCamera& cam );
glm::mat4 calculate_view_proj_matrix( const OrbitCamera& cam );
glm::vec3 calculate_eye_position( const OrbitCamera& cam );

/// TODO: this should probably be moved to another namespace (like orbit_camera)
void process_event( const Context& ctx, const SDL_Event* event, OrbitCamera& cam );
void process_input( OrbitCamera& cam );

} // namespace racecar::camera
