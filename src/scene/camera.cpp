#include "camera.hpp"

#include <SDL3/SDL.h>
#include <glm/gtc/matrix_transform.hpp>

namespace racecar::scene::camera {

glm::mat4 get_view_matrix( Camera& cam ) {
    return cam.view_mat;
}

glm::mat4 get_proj_matrix( Camera& cam ) {
    return glm::perspective( cam.fov_y, cam.aspect_ratio, cam.near_plane, cam.far_plane );
}

glm::mat4 get_view_proj_matrix( Camera& cam ) {
    return get_view_matrix( cam ) * get_proj_matrix( cam );
}

void process_event( const SDL_Event* event, Camera& cam ) {
    // Check for key down events
    if ( event->type == SDL_EVENT_KEY_DOWN ) {
        if ( event->key.key == SDLK_W ) {
            cam.velocity.z = -1;
        }
        if ( event->key.key == SDLK_S ) {
            cam.velocity.z = 1;
        }
        if ( event->key.key == SDLK_A ) {
            cam.velocity.x = -1;
        }
        if ( event->key.key == SDLK_D ) {
            cam.velocity.x = 1;
        }
    }

    // Check for key up events
    if ( event->type == SDL_EVENT_KEY_UP ) {
        if ( event->key.key == SDLK_W ) {
            cam.velocity.z = 0;
        }
        if ( event->key.key == SDLK_S ) {
            cam.velocity.z = 0;
        }
        if ( event->key.key == SDLK_A ) {
            cam.velocity.x = 0;
        }
        if ( event->key.key == SDLK_D ) {
            cam.velocity.x = 0;
        }
    }

    // Check for mouse motion events
    if ( event->type == SDL_EVENT_MOUSE_MOTION ) {
        glm::vec3 direction = glm::normalize( cam.look_at - cam.eye );
        float yaw = std::atan2( direction.z, direction.x );
        float pitch = std::asin( direction.y );

        // xrel and yrel fields remain the same in the motion struct
        yaw += static_cast<float>( event->motion.xrel / 200.f );
        pitch -= static_cast<float>( event->motion.yrel / 200.f );
        pitch = glm::clamp( pitch, glm::radians( -89.0f ), glm::radians( 89.0f ) );

        // Update camera properties
        float distance = glm::distance( cam.look_at, cam.eye );
        glm::vec3 new_eye;
        new_eye.x = cam.look_at.x + distance * cos( yaw ) * cos( pitch );
        new_eye.y = cam.look_at.y + distance * sin( pitch );
        new_eye.z = cam.look_at.z + distance * sin( yaw ) * cos( pitch );

        cam.eye = new_eye;
        cam.forward = glm::normalize( cam.look_at - cam.eye );

        // By default, rotate around 1 unit in front of the camera
        cam.look_at = cam.eye + cam.forward;
        glm::vec3 world_up = glm::vec3( 0.0f, 1.0f, 0.0f );
        cam.right = glm::normalize( glm::cross( cam.forward, world_up ) );
        cam.up = glm::normalize( glm::cross( cam.right, cam.forward ) );
        cam.view_mat = glm::lookAt( cam.eye, cam.look_at, cam.up );
    }
}

}  // namespace racecar::scene::camera
