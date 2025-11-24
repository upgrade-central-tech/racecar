module bindings;

public struct Uniforms {
    public float4x4 inverse_proj;
    public float4x4 inverse_view;
    public float3 camera_position;
    public float3 sun_direction;
    public float radiance_exposure;
}

layout( binding = 0, set = 0 ) public ConstantBuffer<Uniforms> uniforms;

layout( binding = 0, set = 1 ) public Texture2D<float4> irradiance_lut;
layout( binding = 1, set = 1 ) public Texture3D<float4> scattering_lut;
layout( binding = 2, set = 1 ) public Texture2D<float4> transmittance_lut;

layout( binding = 0, set = 2 ) public SamplerState sampler;


