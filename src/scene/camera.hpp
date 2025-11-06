#pragma once
#include <SDL3/SDL_events.h>
#include <glm/glm.hpp>

namespace racecar::scene {

struct Camera {
  glm::vec3 eye;
  glm::vec3 velocity;

  // Optional, for DOF and centering camera on the car
  glm::vec3 lookAt;

  glm::mat4 viewMat;
  glm::vec3 forward;
  glm::vec3 up;
  glm::vec3 right;

  // Not loaded from GLTF
  double fovy;
  // In radians
  double aspectRatio;
  double nearPlane;
  double farPlane;
};

glm::mat4 getViewMatrix(Camera& cam);
glm::mat4 getProjMatrix(Camera& cam);
glm::mat4 getViewProjMatrix(Camera& cam);
void processSDLEvent(SDL_Event& e, Camera& cam);

}  // namespace racecar::scene
