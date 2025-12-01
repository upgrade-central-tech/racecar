#include "../common.slang"

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct VertexOutput {
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv;
};

// tesselation structs
struct HullOutput {
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv;
};

struct PatchConstantOutput {
    float edges[4] : SV_TessFactor;
    float inside[2] : SV_InsideTessFactor;
};

struct DomainOutput {
    float4 sv_position : SV_Position;
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv;
};

struct FragmentOutput {
    float4 position : SV_Target0;
    float4 normal : SV_Target1;
};


layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_buffer_data;

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    VertexOutput output;
    output.position = mul( camera_buffer_data.model, float4( input.position, 1.0 ) ).xyz;

    float3x3 normal_matrix = (float3x3)transpose( camera_buffer_data.inv_model );
    float3 normal = normalize( mul( normal_matrix, input.normal ) );

    output.normal = normal;

    output.tangent = float4( 0.0, 0.0, 0.0, 0.0 );
    output.uv = float2( 0.0, 0.0 );

    return output;
}


PatchConstantOutput constants( InputPatch<VertexOutput, 4> patch )
{
    PatchConstantOutput pc;

    pc.edges[0] = 4.0;
    pc.edges[1] = 4.0;
    pc.edges[2] = 4.0;
    pc.edges[3] = 4.0;

    pc.inside[0] = 4.0;
    pc.inside[1] = 4.0;

    return pc;
}


[shader( "hull" )]
[domain( "quad" )]
[patchconstantfunc( "constants" )]
[partitioning( "integer" )]
[outputtopology( "triangleccw" )]
[outputcontrolpoints( 4 )]
HullOutput ts_control_main(InputPatch<VertexOutput, 4> patch, uint i : SV_OutputControlPointID)
{
    HullOutput output;

    output.position = patch[i].position;
    output.normal = patch[i].normal;
    output.tangent = patch[i].tangent;
    output.uv = patch[i].uv;

    return output;
}

[shader( "domain" )]
[domain( "quad" )]
DomainOutput ts_eval_main( PatchConstantOutput pc, const OutputPatch<HullOutput, 4> patch, float2 uv : SV_DomainLocation )
{
    DomainOutput outVal;

    float3 p =
        patch[0].position * ( 1 - uv.x ) * ( 1 - uv.y ) +
        patch[1].position * ( uv.x ) * ( 1 - uv.y ) +
        patch[3].position * ( uv.x ) * ( uv.y ) +
        patch[2].position * ( 1 - uv.x ) * ( uv.y );

    float3 n =
        patch[0].normal * ( 1 - uv.x ) * ( 1 - uv.y ) +
        patch[1].normal * ( uv.x ) * ( 1 - uv.y ) +
        patch[3].normal * ( uv.x ) * ( uv.y ) +
        patch[2].normal * ( 1 - uv.x ) * ( uv.y );

    float4 tan =
        patch[0].tangent * ( 1 - uv.x ) * ( 1 - uv.y ) +
        patch[1].tangent * ( uv.x ) * ( 1 - uv.y ) +
        patch[3].tangent * ( uv.x ) * ( uv.y ) +
        patch[2].tangent * ( 1 - uv.x ) * ( uv.y );

    float2 t =
        patch[0].uv * ( 1 - uv.x ) * ( 1 - uv.y ) +
        patch[1].uv * ( uv.x ) * ( 1 - uv.y ) +
        patch[3].uv * ( uv.x ) * ( uv.y ) +
        patch[2].uv * ( 1 - uv.x ) * ( uv.y );

    outVal.sv_position = mul( camera_buffer_data.mvp, float4( p, 1.0 ) );
    outVal.position = p;
    outVal.normal = n;
    outVal.tangent = tan;
    outVal.uv = t;

    return outVal;
}

[shader( "fragment" )]
FragmentOutput fs_main( DomainOutput in )
{
    FragmentOutput output;

    // Stencil is defined as such:
    // 0 - nothing shaded
    // 1 - car shading
    // 2 - terrain shading
    float stencil = TERRAIN_ID;

    output.position = float4( in.position.rgb, stencil );
    output.normal = float4( in.normal.rgb, 1.0f );

    return output;
}


