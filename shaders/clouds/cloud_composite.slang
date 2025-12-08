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

struct CloudUniformData {
    float4x4 inverse_proj;
    float4x4 inverse_view;
    float3 camera_position;
    float cloud_offset_x;
    float3 sun_direction;
    float cloud_offset_y;
};

struct DebugData {
    bool enable_albedo_map;
    bool enable_normal_map;
    bool enable_roughness_metal_map;
    bool normals_only;
    bool albedo_only;
    bool roughness_metal_only;
}

layout( binding = 0, set = 0 ) ConstantBuffer<CloudUniformData> clouds_buffer_data;
layout( binding = 1, set = 0 ) ConstantBuffer<DebugData> debug_data;

layout( binding = 0, set = 1 ) Texture2D<float4> cloud_buffer;

layout( binding = 0, set = 2 ) SamplerState linear_sampler;
layout( binding = 1, set = 2 ) SamplerState linear_mirrored_repeat_sampler;

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    VertexOutput output;

    let clip_position = float4( input.position, 1.f );
    let view_position = mul( clouds_buffer_data.inverse_proj, clip_position );
    let world_position = mul( clouds_buffer_data.inverse_view, float4( view_position.xyz, 1.f ) );

    output.sv_position = clip_position;
    output.position = clip_position.xyz;

    return output;
}

[shader( "fragment" )]
float4 fs_main( VertexOutput in )
    : SV_Target
{
    float2 uv = 0.5f * in.position.xy + 0.5f;

    float4 clouds = cloud_buffer.SampleLevel( linear_sampler, uv, 0 );
    clouds.a = lerp( clouds.a, 0.5f, 0.5f );

    return clouds;
}


