module bindings;

public struct Uniforms {
    public float4x4 inverse_proj;
    public float4x4 inverse_view;
    public float3 camera_position;
    public float exposure;
    public float3 sun_direction;
    public float sun_size_x;
    public float3 white_point;
    public float sun_size_y;
    public float4 earth_center;
}

public ConstantBuffer<Uniforms> uniforms;

public Texture2D<float4> irradiance_lut;
public Texture3D<float4> scattering_lut;
public Texture2D<float4> transmittance_lut;

public SamplerState sampler;


