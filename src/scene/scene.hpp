#pragma once

#include "camera.hpp"
#include <glm/glm.hpp>
#include <tiny_gltf.h>

#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace racecar::scene {

struct Material {
  // TODO
};

/// A primitive is a basic association of geometry data along with a single material.
/// We assume a vertex attribute data is stored in a buffer that contains all attribute
/// data and index data.
struct Primitive {
  int material_id = -1;
  // Offsets are in bytes. Pos, nor, and uv data are assumed to be floats in vec3, vec3, and vec2s
  int pos_offset = -1;
  int nor_offset = -1;
  int uv_offset = -1;
  int col_offset = -1;
  // Index data can be a unsigned short uint_16t or an unsigned int uint_32t
  int ind_offset = -1;
  // Index count is in actual indices, not in bytes.
  size_t ind_count = 0;
  // True if indices are uint_16t, and false if indices are uint_32t
  bool sixteen_bit_indices = true;
  size_t buffer_id = -1;
};

/// A mesh is divided into primitive surfaces that may each have a different material
struct Mesh {
  std::vector<Primitive> primitives;
};

// Each node will have either a mesh or a camera
struct Node {
  std::variant<std::monostate, std::unique_ptr<Mesh>, std::unique_ptr<Camera>> data;

  // transforms are local
  glm::mat4 transform = glm::mat4();
  glm::mat4 inv_transform = glm::mat4();
  glm::mat4 inv_transpose = glm::mat4();

  Node* parent = nullptr;
  std::vector<Node*> children;
};

struct Scene {
  std::vector<std::unique_ptr<Node>> nodes;
  Camera* main_camera = nullptr;
};

bool loadBinaryJson(std::string filepath, Scene& scene);

}  // namespace racecar::scene
