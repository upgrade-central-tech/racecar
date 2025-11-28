struct DebugData {
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
}

layout( binding = 0, set = 1 ) ConstantBuffer<DebugData> debug;


