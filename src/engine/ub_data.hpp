#pragma once

#include <glm/glm.hpp>

/// Stores all uniform buffer structs used for different layouts and shaders. Ensure that all
/// uniform structs are 16 byte aligned!
namespace racecar::ub_data {

/// Camera data
struct Camera {
    glm::mat4 mvp = {};
    glm::mat4 prev_mvp = {};
    glm::mat4 model = glm::mat4( 1.f );
    glm::mat4 inv_model = {};
    glm::mat4 inv_vp = {};
    glm::mat4 view_mat = {};
    glm::mat4 proj_mat = {};
    glm::mat4 inv_proj = {};
    glm::vec4 camera_pos = {};
    glm::vec4 camera_constants = {};
    glm::vec4 camera_constants1 = {};
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
    uint32_t ray_traced_shadows = 0;
};

struct AOData {
    // Pack: thickness, radius, offset, null
    glm::vec4 packed_floats0;
    // Pack: enable ao, null, null, null
    glm::vec4 packed_floats1;
};

struct OctahedralData {
    // Pack: mip level, roughness
    glm::vec4 packedfloats0;
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

struct Material {
    int has_base_color_texture = false;
    /// Typically roughness in G, metallic in B
    int has_metallic_roughness_texture = false;
    int has_normal_texture = false;
    int has_emmisive_texture = false;

    glm::vec4 base_color = glm::vec4( 1 );

    float metallic = 0.f;
    float roughness = 1.f;
    float normal_texture_weight = 1;
    float ior = 1.f;

    glm::vec3 specular_tint = glm::vec3( 1 );
    float specular = 0.f;

    glm::vec3 sheen_tint = glm::vec3( 1 );
    float sheen_roughness = 1.f;

    float sheen_weight = 0.f;
    float transmission = 0.f;
    float clearcoat = 0.f;
    float clearcoat_roughness = 1.f;

    glm::vec3 emissive = glm::vec3( 0.f );
    // 0 if lit, 1 if unlit
    int unlit = false;

    float glintiness = 0.0f;
    float glint_log_density = 22.0f;
    float glint_roughness = 0.518f;
    float glint_randomness = 2.0f;
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
    float radiance_exposure = 0.f;
};

struct Clouds {
    glm::mat4 inverse_proj = {};
    glm::mat4 inverse_view = {};
    glm::vec3 camera_position = {};
    float cloud_offset_x = 0.f;
    glm::vec3 sun_direction = {};
    float cloud_offset_y = 0.f;
};

struct TerrainData {
    float gt7_local_shadow_strength;
    float wetness = 0.0f;
    float snow = 0.0f;

    // debug info
    bool enable_gt7_ao;
    bool shadowing_only;
    bool roughness_only;
};

struct Tonemapping {
    int mode = 0;
    float hdr_target_luminance = 0.f;
};

struct Bloom {
    uint32_t enable = 0.f;
    float threshold = 0.f;
    float filter_radius = 0.f;
};

struct AA {
    int mode = 0;
};

struct ModelMat {
    glm::mat4 model_mat = {};
    glm::mat4 inv_model_mat = {};
    glm::mat4 prev_model_mat = {};
};

} // namespace racecar::uniform_buffer
