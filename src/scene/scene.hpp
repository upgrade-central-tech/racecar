#pragma once

#include "../geometry/mesh.hpp"

#include <glm/glm.hpp>
#include <tiny_gltf.h>

#include <filesystem>
#include <memory>
#include <string>

namespace racecar::scene {

enum class MaterialType {
    DEFAULT,
    PBR_ALBEDO_MAP,
};

enum class ColorSpace { SRGB, UNORM, SFLOAT };

struct Material {
    glm::vec3 base_color;
    std::optional<int> base_color_texture_index = std::nullopt;
    float metallic = 0.f;
    float roughness = 1.f;

    /// Typically roughness in G, metallic in B
    std::optional<int> metallic_roughness_texture_index = std::nullopt;

    float specular = 0.f;
    glm::vec3 specular_tint = glm::vec3( 1 );
    float ior = 1.f;

    float sheen_weight = 0.f;
    glm::vec3 sheen_tint = glm::vec3( 1 );
    float sheen_roughness = 1.f;

    float transmission = 0.f;
    float clearcoat = 0.f;
    float clearcoat_roughness = 1.f;

    glm::vec3 emissive = glm::vec3( 0.f );
    std::optional<int> emmisive_texture_index = std::nullopt;
    bool unlit = false;

    std::optional<int> normal_texture_index = std::nullopt;
    int normal_texture_weight = 1;

    // Low priority, usually R channel of rough-metal
    std::optional<int> occulusion_texture_index = std::nullopt;

    bool double_sided = true; // Low-priority
    MaterialType type = MaterialType::PBR_ALBEDO_MAP; // Can define as needed for easy switching.
};

struct Texture {
    std::optional<vk::mem::AllocatedImage> data;

    int width = 0;
    int height = 0;

    int bits_per_channel = 0; ///< Can only be 8, 16, or 32.
    int num_channels = 0;

    ColorSpace color_space = ColorSpace::UNORM;
};

/// A primitive is a basic association of geometry data along with a single material.
/// We assume a vertex attribute data is stored in a buffer that contains all attribute
/// data and index data.
struct Primitive {
    int material_id = -1;
    ///< Offsets are for the out_vertices array.
    int vertex_offset = -1;
    ///< Index data can be a unsigned short uint_16t or an unsigned int uint_32t.
    int ind_offset = -1;
    /// Index count is in actual indices, not in bytes.
    size_t ind_count = 0;
    bool is_indexed = true;
};

/// A mesh is divided into primitive surfaces that may each have a different material
struct Mesh {
    std::vector<Primitive> primitives;
};

struct Node {
    std::optional<std::unique_ptr<scene::Mesh>> mesh;

    /// transforms are local
    glm::mat4 transform;
    glm::mat4 inv_transform;
    glm::mat4 inv_transpose;

    Node* parent = nullptr;
    std::vector<Node*> children;
};

struct Scene {
    std::vector<std::unique_ptr<Node>> nodes;
    std::vector<Material> materials;
    std::vector<Texture> textures;

    std::optional<size_t> hdri_index;
};

void load_gltf( vk::Common& vulkan, engine::State& engine, std::filesystem::path file_path,
    Scene& scene, std::vector<geometry::Vertex>& out_vertices, std::vector<uint32_t>& out_indices );

bool load_hdri( vk::Common vulkan, engine::State& engine, std::string file_path, Scene& scene );

} // namespace racecar::scene
