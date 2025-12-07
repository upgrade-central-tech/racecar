#pragma once

static const int CAR_ID = 1;
static const int TERRAIN_ID = 2;

static const float PI = 3.14159265359;

struct CameraBufferData {
    float4x4 mvp;
    float4x4 prev_mvp;
    float4x4 model;
    float4x4 inv_model;
    float4x4 inv_vp;
    float4x4 view_mat;
    float4x4 proj_mat;
    float4x4 inv_proj;
    float4 camera_pos;
    // Pack: near plane, far plane, aspect ratio, fov_Y
    float4 camera_constants;

    // Pack: modded frame index [0, 15], null, null, null
    float4 camera_constants1;
};

struct AOData {
    // Pack: thickness, radius, offset, null
    float4 packed_floats0;
    // Pack: enable ao, or don't
    float4 packed_floats1;
}

struct AASettings {
    int mode;
};

public struct DebugData {
    float4 color;

    // Pack: roughness, metallic, clearcoat roughness, and clearcoat weight respsectively
    float4 packed_data0;
    float4 sun_direction;

    bool enable_albedo_map;
    bool enable_normal_map;
    bool enable_roughness_metal_map;
    bool normals_only;
    bool albedo_only;
    bool roughness_metal_only;

    bool enable_bloom;
    bool ray_traced_shadows;
}

struct TerrainData {
    float gt7_local_shadow_strength;
    float wetness;
    float snow;

    // debug info
    bool enable_gt7_ao;
    bool shadowing_only;
    bool roughness_only;
}

struct MaterialData {
    int has_base_color_texture;
    int has_metallic_roughness_texture;
    int has_normal_texture;
    int has_emmisive_texture;

    float4 base_color;

    float metallic;
    float roughness;
    float normal_texture_weight;
    float ior;

    float3 specular_tint;
    float specular;

    float3 sheen_tint;
    float sheen_roughness;

    float sheen_weight;
    float transmission;
    float clearcoat;
    float clearcoat_roughness;

    float3 emissive;
    // 0 if lit, 1 if unlit
    int unlit = false;

    // glint params
    // Glint:
    float glintiness; // 1.0f
    float glint_log_density; // 18.0f
    float glint_roughness; // 0.28f
    float glint_randomness; // 2.0f
};

struct ModelMatData {
    float4x4 model_mat;
    float4x4 inv_model_mat;
    float4x4 prev_model_mat;
};


