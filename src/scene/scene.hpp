#pragma once

#include "../geometry/mesh.hpp"
#include "camera.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <tiny_gltf.h>
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
    /// Offsets are for the out_vertices array.
    int vertex_offset = -1;
    size_t vertex_count = 0;
    /// Index data can be a unsigned short uint_16t or an unsigned int uint_32t.
    int ind_offset = -1;
    /// Index count is in actual indices, not in bytes.
    size_t ind_count = 0;

    bool is_indexed = true;
};

/// A mesh is divided into primitive surfaces that may each have a different material
struct Mesh {
    std::vector<Primitive> primitives;
};

/// Each node will have either a mesh or a camera
struct Node {
    std::variant<std::monostate, std::unique_ptr<Mesh>, std::unique_ptr<Camera>> data;

    /// transforms are local
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

bool load_binary_json( std::string filepath,
                       Scene& scene,
                       std::vector<geometry::Vertex>& out_vertices,
                       std::vector<uint32_t>& out_indices );

}  // namespace racecar::scene
