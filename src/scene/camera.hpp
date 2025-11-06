#pragma once
#include <SDL3/SDL_events.h>
#include <glm/glm.hpp>

namespace racecar::scene {

struct Camera {
  glm::vec3 eye;
  glm::vec3 velocity;

  /// Optional, for DOF and centering camera on the car
  glm::vec3 look_at;

  glm::mat4 view_mat;
  glm::vec3 forward;
  glm::vec3 up;
  glm::vec3 right;

  /// Not loaded from GLTF
  double fovy;
  /// In radians
  double aspect_ratio;
  double near_plane;
  double far_plane;
};

glm::mat4 getViewMatrix(Camera& cam);
glm::mat4 getProjMatrix(Camera& cam);
glm::mat4 getViewProjMatrix(Camera& cam);
void processSDLEvent(SDL_Event& e, Camera& cam);

}  // namespace racecar::scene
