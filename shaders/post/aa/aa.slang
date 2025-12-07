#include "../../common.slang"

layout( binding = 0, set = 0 ) Texture2D<float4> scene_color;
layout( binding = 1, set = 0 ) Texture2D<float4> scene_history;
layout( binding = 2, set = 0 ) Texture2D<float4> gbuffer_depth;
layout( binding = 3, set = 0 ) RWTexture2D<float4> out_color;
layout( binding = 4, set = 0 ) SamplerState linear_sampler;
layout( binding = 5, set = 0 ) ConstantBuffer<CameraBufferData> camera_data;
layout( binding = 6, set = 0 ) ConstantBuffer<AASettings> aa_settings;

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
func cs_main( uint2 thread_id: SV_DispatchThreadID )->void
{
    uint2 pixel = thread_id.xy;

    if ( aa_settings.mode == 0 ) {
        out_color[pixel] = scene_color[pixel];
        return;
    }

    // Hardcoded screen dimensions
    float2 uv = ( float2( pixel ) + 0.5f ) / float2( 1920.0f, 1080.0f );

    // Reconstruct world-pos
    float depth = gbuffer_depth[pixel].r;

    float4 curr_clip = float4( uv * 2.0f - 1.0f, depth, 1.0f );
    float4 curr_world_pos = mul( camera_data.inv_vp, curr_clip );
    curr_world_pos.xyz /= curr_world_pos.w;

    // Reproject into prev-frame
    float4 prev_clip = mul( camera_data.prev_vp, float4( curr_world_pos.xyz, 1.0f ) );
    float2 prev_uv = prev_clip.xy / prev_clip.w * 0.5f + 0.5f;

    bool invalid = any( prev_uv < 0.0f ) || any( prev_uv > 1.0f );

    float3 prev_color = select(
        invalid, float3( 0.0f ), scene_history.SampleLevel( linear_sampler, prev_uv, 0 ).rgb );
    float3 curr_color = scene_color[pixel].rgb;

    // Neighborhood clamp process
    float3 n0 = scene_color.Load( int3( pixel + int2( -1, -1 ), 0 ) ).rgb;
    float3 n1 = scene_color.Load( int3( pixel + int2( 1, -1 ), 0 ) ).rgb;
    float3 n2 = scene_color.Load( int3( pixel + int2( -1, 1 ), 0 ) ).rgb;
    float3 n3 = scene_color.Load( int3( pixel + int2( 1, 1 ), 0 ) ).rgb;

    float3 nmin = min( min( n0, n1 ), min( n2, n3 ) );
    float3 nmax = max( max( n0, n1 ), max( n2, n3 ) );
    prev_color = clamp( prev_color, nmin, nmax );

    // Motion blending
    float2 motion = ( uv - prev_uv );
    float motion_mag = length( motion );
    float motion_boost = 0.35f;
    float blend = saturate( 1.0f - motion_mag * motion_boost );

    float modulation_factor = lerp( 0.1f, 0.9f, blend );

    float3 out = lerp( curr_color, prev_color, modulation_factor );

    out_color[pixel] = float4( out, 1.0f );
}

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
func cs_write_history( uint2 thread_id: SV_DispatchThreadID )->void
{
    uint2 pixel = thread_id.xy;

    out_color[pixel] = scene_color[pixel];
}


