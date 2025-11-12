#include "orbit_camera.hpp"

#include <SDL3/SDL.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace racecar::camera {

/// Offset by a little epsilon.
constexpr float MAX_POLAR_ANGLE = glm::half_pi<float>() - 0.001f;
constexpr float MIN_RADIUS = 0.1f;
constexpr float MOVEMENT_SPEED = 1.f / 200.f;

void rotate_azimuth( OrbitCamera& cam, float radians ) {
    cam.azimuth += radians;
    cam.azimuth = std::fmod( cam.azimuth, glm::two_pi<float>() );

    if ( cam.azimuth < 0.f ) {
        cam.azimuth = glm::two_pi<float>() + cam.azimuth;
    }
}

void rotate_polar( OrbitCamera& cam, float radians ) {
    cam.polar = glm::clamp( cam.polar + radians, -MAX_POLAR_ANGLE, MAX_POLAR_ANGLE );
}

void move_horizontal( OrbitCamera& cam, float delta ) {
    glm::vec3 eye = calculate_eye_position( cam );
    glm::vec3 view = glm::normalize( cam.center - eye );
    glm::vec3 strafe = glm::normalize( glm::cross( view, cam.up ) );

    cam.center += strafe * delta;
}

void move_vertical( OrbitCamera& cam, float delta ) {
    cam.center += cam.up * delta;
}

void zoom( OrbitCamera& cam, float delta ) {
    cam.radius = std::max( cam.radius - delta, MIN_RADIUS );
}

glm::mat4 calculate_view_matrix( const OrbitCamera& cam ) {
    return glm::lookAt( calculate_eye_position( cam ), cam.center, cam.up );
}

glm::vec3 calculate_eye_position( const OrbitCamera& cam ) {
    float x = cam.center.x + cam.radius * std::cos( cam.polar ) * std::cos( cam.azimuth );
    float y = cam.center.y + cam.radius * std::sin( cam.polar );
    float z = cam.center.z + cam.radius * std::cos( cam.polar ) * std::sin( cam.azimuth );

    return glm::vec3( x, y, z );
}

glm::mat4 calculate_proj_matrix( const OrbitCamera& cam ) {
    return glm::perspective( cam.fov_y, cam.aspect_ratio, cam.near_plane, cam.far_plane );
}

glm::mat4 calculate_view_proj_matrix( const OrbitCamera& cam ) {
    return calculate_view_matrix( cam ) * calculate_proj_matrix( cam );
}

void process_event( const SDL_Event* event, OrbitCamera& cam ) {
    switch ( event->type ) {
        case SDL_EVENT_MOUSE_MOTION: {
            if ( event->motion.state & SDL_BUTTON_LMASK ) {
                camera::rotate_polar( cam, event->motion.yrel * MOVEMENT_SPEED );
                camera::rotate_azimuth( cam, event->motion.xrel * MOVEMENT_SPEED );
            } else if ( event->motion.state & SDL_BUTTON_MMASK ) {
                camera::move_horizontal( cam, event->motion.xrel * -MOVEMENT_SPEED );
                camera::move_vertical( cam, event->motion.yrel * MOVEMENT_SPEED );
            }

            break;
        }

        case SDL_EVENT_MOUSE_WHEEL: {
            if ( event->wheel.y > 0 ) {
                camera::zoom( cam, 0.1f );
            } else if ( event->wheel.y < 0 ) {
                camera::zoom( cam, -0.1f );
            }

            break;
        }
    }
}

}  // namespace racecar::camera
