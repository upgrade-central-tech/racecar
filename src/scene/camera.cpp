#include "camera.hpp"

#include <SDL3/SDL.h>
#include <glm/gtc/matrix_transform.hpp>

namespace racecar::scene {

glm::mat4 getViewMatrix(Camera& cam) {
  return cam.viewMat;
}

glm::mat4 getProjMatrix(Camera& cam) {
  return glm::perspective(cam.fovy, cam.aspectRatio, cam.nearPlane, cam.farPlane);
}

glm::mat4 getViewProjMatrix(Camera& cam) {
  return getViewMatrix(cam) * getProjMatrix(cam);
}

void processSDLEvent(SDL_Event& e, Camera& cam) {
  // Check for key down events
  if (e.type == SDL_EVENT_KEY_DOWN) {
    if (e.key.key == SDLK_W) {
      cam.velocity.z = -1;
    }
    if (e.key.key == SDLK_S) {
      cam.velocity.z = 1;
    }
    if (e.key.key == SDLK_A) {
      cam.velocity.x = -1;
    }
    if (e.key.key == SDLK_D) {
      cam.velocity.x = 1;
    }
  }

  // Check for key up events
  if (e.type == SDL_EVENT_KEY_UP) {
    if (e.key.key == SDLK_W) {
      cam.velocity.z = 0;
    }
    if (e.key.key == SDLK_S) {
      cam.velocity.z = 0;
    }
    if (e.key.key == SDLK_A) {
      cam.velocity.x = 0;
    }
    if (e.key.key == SDLK_D) {
      cam.velocity.x = 0;
    }
  }

  // Check for mouse motion events
  if (e.type == SDL_EVENT_MOUSE_MOTION) {
    glm::vec3 direction = glm::normalize(cam.lookAt - cam.eye);
    float yaw = std::atan2(direction.z, direction.x);
    float pitch = std::asin(direction.y);

    // xrel and yrel fields remain the same in the motion struct
    yaw += (float)e.motion.xrel / 200.f;
    pitch -= (float)e.motion.yrel / 200.f;

    pitch = glm::clamp(pitch, glm::radians(-89.0f), glm::radians(89.0f));

    // update camera properties
    glm::vec3 newEye;
    float distance = glm::distance(cam.lookAt, cam.eye);
    newEye.x = cam.lookAt.x + distance * cos(yaw) * cos(pitch);
    newEye.y = cam.lookAt.y + distance * sin(pitch);
    newEye.z = cam.lookAt.z + distance * sin(yaw) * cos(pitch);

    cam.eye = newEye;
    cam.forward = glm::normalize(cam.lookAt - cam.eye);
    // By default, rotate around 1 unit in front of the camera.
    cam.lookAt = cam.eye + cam.forward;
    glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    cam.right = glm::normalize(glm::cross(cam.forward, worldUp));
    cam.up = glm::normalize(glm::cross(cam.right, cam.forward));
    cam.viewMat = glm::lookAt(cam.eye, cam.lookAt, cam.up);
  }
}

}  // namespace racecar::scene
