#include "../common.slang"
#include "cloud_uniforms.slang"

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_buffer_data;
layout( binding = 1, set = 0 ) Texture2D<float4> rotating_cloud_buffer;
layout( binding = 2, set = 0 ) ConstantBuffer<CloudUniformData> clouds_buffer_data;
layout( binding = 3, set = 0 ) SamplerState linear_sampler;
layout( binding = 4, set = 0 ) Texture2D<float4> cloud_history;
layout( binding = 5, set = 0 ) Texture2D<float> cloud_depth_history;
layout( binding = 6, set = 0 ) Texture2D<float> rotating_cloud_depth;

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT0;
    float2 uv : TEXCOORD0;
};

struct VertexOutput {
    float4 sv_position : SV_POSITION;
    float3 position;
};

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    VertexOutput output;

    let clip_position = float4( input.position, 1.f );
    let view_position = mul( clouds_buffer_data.inverse_proj, clip_position );
    let world_position = mul( clouds_buffer_data.inverse_view, float4( view_position.xyz, 1.f ) );

    output.sv_position = clip_position;
    output.position = world_position.xyz;

    return output;
}

struct FragmentOutput {
    float4 cloud_buffer : SV_Target0;
    float cloud_depth : SV_Target1;
}

[shader( "fragment" )]
FragmentOutput fs_main( VertexOutput input )
{
    FragmentOutput output;

    output.cloud_buffer = float4( 1.0, 0.0, 0.0, 1.0 );
    output.cloud_depth = 0.0;

    return output;
}