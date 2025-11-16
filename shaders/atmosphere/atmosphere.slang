struct VertexInput {
    float2 position : POSITION;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    float2 uv;
};

struct Uniforms {
    float4x4 inverse_proj;
    float4x4 inverse_view;
    float3 camera_position;
    float exposure;
    float3 sun_position;
    float size_x;
    float3 white_point;
    float size_y;
    float4 earth_center;
}

layout( binding = 0, set = 0 ) ConstantBuffer<Uniforms> uniforms;

layout( binding = 0, set = 1 ) Texture2D<float4> irradiance_lut;
layout( binding = 1, set = 1 ) Texture3D<float4> scattering_lut;
layout( binding = 2, set = 1 ) Texture2D<float4> transmittance_lut;

layout( binding = 0, set = 2 ) SamplerState sampler;

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    VertexOutput output;
    output.sv_position = float4( input.position, 0.1f, 1.f );
    output.uv = ( input.position + 1.f ) * 0.5f;

    return output;
}

[shader( "fragment" )]
float4 fs_main( VertexOutput input )
    : SV_Target
{
    return float4( transmittance_lut.Sample( sampler, input.uv ).rgb, 1.f );
}


