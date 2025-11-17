struct VertexInput {
    float2 position : POSITION;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    float3 view_ray;
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

ConstantBuffer<Uniforms> uniforms;

Texture2D<float4> irradiance_lut;
Texture3D<float4> scattering_lut;
Texture2D<float4> transmittance_lut;

SamplerState sampler;

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    let clip_position = float4( input.position, 0.f, 1.f );
    let view_position = mul( clip_position, uniforms.inverse_proj );
    let world_position = mul( view_position, uniforms.inverse_view );

    var output : VertexOutput;
    output.sv_position = clip_position;
    output.view_ray = world_position.xyz;

    return output;
}

[shader( "fragment" )]
float4 fs_main( VertexOutput input )
    : SV_Target
{
    return float4( 0.f, 1.f, 0.f, 1.f );
}


