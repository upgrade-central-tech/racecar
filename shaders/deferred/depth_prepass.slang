struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT0;
    float2 uv : TEXCOORD0;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
};

struct CameraBufferData {
    float4x4 mvp;
    float4x4 model;
    float4x4 inv_model;
    float4 camera_pos;
    // Pack: near plane, far plane, aspect ratio, fov_Y
    float4 camera_constants;
};

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_buffer_data;

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    VertexOutput output;
    output.sv_position = mul( camera_buffer_data.mvp, float4( input.position, 1.0 ) );

    return output;
}

struct FragmentOutput {
    float4 position : SV_Target0;
    float4 normal : SV_Target1;
    float4 tangent : SV_Target2;
    float4 uv : SV_Target3;
    float4 albedo : SV_Target4;
    /// roughness, metallic, clearcoat roughness, clearcoat weight
    float4 packed_data : SV_Target5;
}

[shader( "fragment" )]
void fs_main( VertexOutput in ) { }


