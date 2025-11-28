#include "../common.slang"

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv;
};

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_buffer_data;

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    VertexOutput output;
    output.sv_position = mul( camera_buffer_data.mvp, float4( input.position, 1.0 ) );
    output.position = mul( camera_buffer_data.model, float4( input.position, 1.0 ) ).xyz;

    float3x3 normal_matrix = (float3x3)transpose( camera_buffer_data.inv_model );
    float3 normal = normalize( mul( normal_matrix, input.normal ) );

    output.normal = normal;

    return output;
}

struct FragmentOutput {
    float4 position : SV_Target0;
    float4 normal : SV_Target1;
}

[shader( "fragment" )]
FragmentOutput fs_main( VertexOutput in )
{
    FragmentOutput output;

    // Stencil is defined as such:
    // 0 - nothing shaded
    // 1 - car shading
    // 2 - terrain shading
    float stencil = TERRAIN_ID;

    output.position = float4( in.position, stencil );
    output.normal = float4( in.normal, 1.0f );

    return output;
}


