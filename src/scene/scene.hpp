#pragma once

#include "camera.hpp"
#include "glm/glm.hpp"
#include "tiny_gltf.h"

#include <memory>
#include <optional>
#include <string>

namespace racecar::scene {

struct Material {
  // TODO
};

/// A primative is a basic association of geometry data along with a single material.
/// We assume a vertex attribute data is stored in a buffer that contains all attribute
/// data and index data.
struct Primative {
  int materialId = -1;
  // Offsets are in bytes. Pos, nor, and uv data are assumed to be floats in vec3, vec3, and vec2s
  int posOffset = -1;
  int norOffset = -1;
  int uvOffset = -1;
  int colOffset = -1;
  // Index data can be a unsigned short uint_16t or an unsigned int uint_32t
  int indOffset = -1;
  // Index count is in actual indices, not in bytes.
  size_t indCount = 0;
  // True if indices are uint_16t, and false if indices are uint_32t
  bool sixteenBitIndices = true;
  size_t bufferID;
};

/// A mesh is divided into primative surfaces that may each have a different material
struct Mesh {
  std::vector<Primative> primatives;
};

// Each node will have either a mesh or a camera
struct Node {
  std::optional<std::unique_ptr<Mesh>> mesh;
  std::optional<std::unique_ptr<Camera>> camera;

  // transforms are local
  glm::mat4 transform = glm::mat4(0.f);
  glm::mat4 invTransform = glm::mat4(0.f);
  glm::mat4 invTranspose = glm::mat4(0.f);

  Node* parent;
  std::vector<Node*> children;
};

struct Scene {
  std::vector<std::unique_ptr<Node>> nodes;
  Camera* mainCamera = nullptr;
};

bool loadBinaryJson(std::string filepath, Scene& scene);

}  // namespace racecar::scene
