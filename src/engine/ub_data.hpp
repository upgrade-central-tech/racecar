#pragma once

#include <glm/glm.hpp>

/// Stores all uniform buffer structs used for different layouts and shaders. Ensure that all
/// uniform structs are 16 byte aligned!
namespace racecar::ub_data {

/// Camera data
struct Camera {
    glm::mat4 mvp = {};
    glm::mat4 model = glm::mat4( 1.f );
    glm::mat4 inv_model = {};
    glm::vec3 camera_pos = {};
    float padding0 = 0.0f;
    glm::vec3 color = {};
    float padding1 = 0.0f;
};

struct Debug {
    glm::vec4 color = {};
    glm::vec4 packed_data0 = {};
    glm::vec4 sun_direction = {};

    uint32_t enable_albedo_map = 0;
    uint32_t enable_normal_map = 0;
    uint32_t enable_roughness_metal_map = 0;
    uint32_t normals_only = 0;
    uint32_t albedo_only = 0;
    uint32_t roughness_metal_only = 0;

    uint32_t padding0[2];
};

struct SHData {
    // Packs all 9 spherical harmonics
    glm::vec4 coeff0 = {};
    glm::vec4 coeff1 = {};
    glm::vec4 coeff2 = {};
    glm::vec4 coeff3 = {};
    glm::vec4 coeff4 = {};
    glm::vec4 coeff5 = {};
    glm::vec4 coeff6 = {};
};

struct RaymarchBufferData {
    float step_size;
    uint8_t p0[3];
};

struct Atmosphere {
    glm::mat4 inverse_proj = {};
    glm::mat4 inverse_view = {};
    glm::vec3 camera_position = {};
    uint8_t p0 = 0;
    glm::vec3 sun_direction = {};
    uint8_t p1 = 0;
};

} // namespace racecar::uniform_buffer
