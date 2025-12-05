import cloud_bindings;
import cloud_raymarch;

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
}

[shader( "fragment" )]
FragmentOutput fs_main( VertexOutput in )
{
    float3 world_pos = in.position;
    float3 camera_pos = clouds_buffer_data.camera_position.rgb;

    float3 view = normalize( world_pos - camera_pos );

    float4 output_color = raymarch( cumulus_map_LUT, low_freq_noise_LUT,
        linear_mirrored_repeat_sampler, view, world_pos, clouds_buffer_data.sun_direction );

    FragmentOutput output;

    output.cloud_buffer = float4( output_color.rgb * output_color.a, output_color.a );

    return output;
}


