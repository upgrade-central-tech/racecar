#include "../common.slang"

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT0;
    float2 uv : TEXCOORD0;
    float2 id : IDS;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    float3 position;
    float4 prev_clip_pos;
    float4 curr_clip_pos;
    float3 normal;
    float4 tangent;
    float2 uv;
};

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_buffer_data;
layout( binding = 0, set = 1 ) ConstantBuffer<ModelMatData> model_mat_data;

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    VertexOutput output;
    float4 clip_pos = mul(
        mul( camera_buffer_data.mvp, model_mat_data.model_mat ), float4( input.position, 1.0 ) );

    float4 curr_clip_pos = mul(
        mul( camera_buffer_data.unjittered_mvp, model_mat_data.model_mat ),
        float4( input.position, 1.0f ) );

    float4 prev_clip_pos = mul( mul( camera_buffer_data.prev_mvp, model_mat_data.prev_model_mat ),
        float4( input.position, 1.0f ) );

    output.sv_position = clip_pos;
    output.curr_clip_pos = curr_clip_pos;
    output.prev_clip_pos = prev_clip_pos;

    output.position = mul(
        mul( camera_buffer_data.model, model_mat_data.model_mat ), float4( input.position, 1.0 ) )
                          .xyz;

    float3x3 normal_matrix
        = (float3x3)transpose( mul( model_mat_data.inv_model_mat, camera_buffer_data.inv_model ) );
    float3 normal = normalize( mul( normal_matrix, input.normal ) );
    float3 tangent = normalize( mul( normal_matrix, input.tangent.xyz ) );
    tangent = normalize( tangent - normal * dot( normal, tangent ) );

    output.normal = normal;
    output.tangent = float4( tangent, input.tangent.w );
    output.uv = input.uv;

    return output;
}

[shader( "fragment" )]
float4 fs_main( VertexOutput in ) : SV_Target
{
    return float4( 1.0, 0.0, 0.0, 1.0 );
}


