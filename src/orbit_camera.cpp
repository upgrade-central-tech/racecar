#include "orbit_camera.hpp"

#include <SDL3/SDL.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace racecar::camera {

/// Offset by a little epsilon.
constexpr float MAX_POLAR_ANGLE = glm::half_pi<float>() - 0.001f;
constexpr float MIN_RADIUS = 0.1f;
constexpr float DRAG_SPEED = 1.f / 200.f;
constexpr float TRANSLATE_SPEED = 0.05f;

void rotate_azimuth( OrbitCamera& cam, float radians )
{
    cam.azimuth += radians;
    cam.azimuth = std::fmod( cam.azimuth, glm::two_pi<float>() );

    if ( cam.azimuth < 0.f ) {
        cam.azimuth = glm::two_pi<float>() + cam.azimuth;
    }
}

void rotate_polar( OrbitCamera& cam, float radians )
{
    cam.polar = glm::clamp( cam.polar + radians, -MAX_POLAR_ANGLE, MAX_POLAR_ANGLE );
}

void move_along_view( OrbitCamera& cam, float delta )
{
    glm::vec3 view = glm::normalize( cam.center - calculate_eye_position( cam ) );
    cam.center += view * delta;
}

void move_horizontal( OrbitCamera& cam, float delta )
{
    glm::vec3 eye = calculate_eye_position( cam );
    glm::vec3 view = glm::normalize( cam.center - eye );
    glm::vec3 strafe = glm::normalize( glm::cross( view, cam.up ) );

    cam.center += strafe * delta;
}

void move_vertical( OrbitCamera& cam, float delta ) { cam.center += cam.up * delta; }

void zoom( OrbitCamera& cam, float delta )
{
    cam.radius = std::max( cam.radius - delta, MIN_RADIUS );
}

glm::mat4 calculate_view_matrix( const OrbitCamera& cam )
{
    return glm::lookAt( calculate_eye_position( cam ), cam.center, cam.up );
}

glm::vec3 calculate_eye_position( const OrbitCamera& cam )
{
    float x = cam.center.x + cam.radius * std::cos( cam.polar ) * std::cos( cam.azimuth );
    float y = cam.center.y + cam.radius * std::sin( cam.polar );
    float z = cam.center.z + cam.radius * std::cos( cam.polar ) * std::sin( cam.azimuth );

    return glm::vec3( x, y, z );
}

glm::mat4 calculate_proj_matrix( const OrbitCamera& cam )
{
    return glm::perspective( cam.fov_y, cam.aspect_ratio, cam.near_plane, cam.far_plane );
}

glm::mat4 calculate_view_proj_matrix( const OrbitCamera& cam )
{
    return calculate_view_matrix( cam ) * calculate_proj_matrix( cam );
}

void process_event( const Context& ctx, const SDL_Event* event, OrbitCamera& cam )
{
    switch ( event->type ) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        SDL_SetWindowRelativeMouseMode( ctx.window, true );
        break;
    }

    case SDL_EVENT_MOUSE_BUTTON_UP: {
        SDL_SetWindowRelativeMouseMode( ctx.window, false );
        break;
    }

    case SDL_EVENT_MOUSE_MOTION: {
        if ( event->motion.state & SDL_BUTTON_LMASK ) {
            camera::rotate_polar( cam, event->motion.yrel * DRAG_SPEED );
            camera::rotate_azimuth( cam, event->motion.xrel * DRAG_SPEED );
        } else if ( event->motion.state & SDL_BUTTON_MMASK ) {
            camera::move_horizontal( cam, event->motion.xrel * -DRAG_SPEED );
            camera::move_vertical( cam, event->motion.yrel * DRAG_SPEED );
        }

        break;
    }

    case SDL_EVENT_MOUSE_WHEEL: {
        if ( event->wheel.y > 0.f ) {
            camera::zoom( cam, 0.1f );
        }

        if ( event->wheel.y < 0.f ) {
            camera::zoom( cam, -0.1f );
        }

        break;
    }
    }
}

void process_input( OrbitCamera& cam )
{
    const bool* key_states = SDL_GetKeyboardState( nullptr );
    glm::vec3 direction = {};

    if ( key_states[SDL_SCANCODE_W] ) {
        direction.z += TRANSLATE_SPEED;
    }
    if ( key_states[SDL_SCANCODE_S] ) {
        direction.z += -TRANSLATE_SPEED;
    }

    if ( key_states[SDL_SCANCODE_A] ) {
        direction.x += -TRANSLATE_SPEED;
    }
    if ( key_states[SDL_SCANCODE_D] ) {
        direction.x += TRANSLATE_SPEED;
    }

    if ( key_states[SDL_SCANCODE_Q] ) {
        direction.y += -TRANSLATE_SPEED;
    }
    if ( key_states[SDL_SCANCODE_E] ) {
        direction.y += TRANSLATE_SPEED;
    }

    if ( direction.x != 0.f ) {
        camera::move_horizontal( cam, direction.x );
    }
    if ( direction.y != 0.f ) {
        camera::move_vertical( cam, direction.y );
    }
    if ( direction.z != 0.f ) {
        camera::move_along_view( cam, direction.z );
    }
}

} // namespace racecar::camera
