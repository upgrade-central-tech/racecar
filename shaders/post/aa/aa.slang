#include "../../common.slang"

layout( binding = 0, set = 0 ) Texture2D<float4> scene_color;
layout( binding = 1, set = 0 ) Texture2D<float4> scene_history;
layout( binding = 2, set = 0 ) Texture2D<float4> gbuffer_depth;
layout( binding = 3, set = 0 ) Texture2D<float2> gbuffer_velocity;
layout( binding = 4, set = 0 ) RWTexture2D<float4> out_color;
layout( binding = 5, set = 0 ) SamplerState linear_sampler;
layout( binding = 6, set = 0 ) ConstantBuffer<CameraBufferData> camera_data;
layout( binding = 7, set = 0 ) ConstantBuffer<AASettings> aa_settings;

float3 RGB_to_YCoCg( float3 c )
{
    float Y = dot( c, float3( 0.25, 0.50, 0.25 ) );
    float Co = dot( c, float3( 0.50, 0.00, -0.50 ) );
    float Cg = dot( c, float3( -0.25, 0.50, -0.25 ) );
    return float3( Y, Co, Cg );
}

float3 YCoCg_to_RGB( float3 c )
{
    float Y = c.x;
    float Co = c.y;
    float Cg = c.z;
    float R = Y + Co - Cg;
    float G = Y + Cg;
    float B = Y - Co - Cg;
    return float3( R, G, B );
}

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
    uint width;
    uint height;
    scene_color.GetDimensions( width, height );
    float2 uv = ( float2( pixel ) + 0.5f ) / float2( width, height );
    float velocity = 0.0f; // gbuffer_velocity[pixel];

    // computes mag sq, check if valid.
    // If there's no velocity, do fallback method
    if ( dot( velocity, velocity ) <= 1e-10 ) {
        // Reconstruct world-pos
        float depth = gbuffer_depth[pixel].r;

        float4 curr_clip = float4( uv * 2.0f - 1.0f, depth, 1.0f );
        float4 curr_viewpos = mul( camera_data.inv_vp, curr_clip );
        curr_viewpos.xyz /= curr_viewpos.w;

        // Reproject into prev-frame
        float4 prev_clip = mul( camera_data.prev_mvp, float4( curr_viewpos.xyz, 1.0f ) );
        velocity = ( prev_clip.xy / prev_clip.w * 0.5f + 0.5f ) - uv;
    }

    float2 prev_uv = uv + velocity;
    bool invalid = any( prev_uv < 0.0f ) || any( prev_uv > 1.0f );

    float3 prev_color = select(
        invalid, float3( 0.0f ), scene_history.SampleLevel( linear_sampler, prev_uv, 0 ).rgb );
    float3 curr_color = scene_color[pixel].rgb;

    // Neighborhood clamp process
    float3 n0 = RGB_to_YCoCg( scene_color.Load( int3( pixel + int2( -1, -1 ), 0 ) ).rgb );
    float3 n1 = RGB_to_YCoCg( scene_color.Load( int3( pixel + int2( 1, -1 ), 0 ) ).rgb );
    float3 n2 = RGB_to_YCoCg( scene_color.Load( int3( pixel + int2( -1, 1 ), 0 ) ).rgb );
    float3 n3 = RGB_to_YCoCg( scene_color.Load( int3( pixel + int2( 1, 1 ), 0 ) ).rgb );

    float3 nmin = min( min( n0, n1 ), min( n2, n3 ) );
    float3 nmax = max( max( n0, n1 ), max( n2, n3 ) );
    float3 prev_ycocg = RGB_to_YCoCg( prev_color );

    prev_color = clamp( prev_ycocg, nmin, nmax );

    // Motion blending
    float2 motion = ( uv - prev_uv );
    float motion_mag = length( motion );
    const float motion_boost = 0.35f;
    float blend = saturate( 1.0f - motion_mag * motion_boost );

    float modulation_factor = lerp( 0.1f, 0.9f, blend );

    float3 out = lerp( curr_color, YCoCg_to_RGB( prev_color ), modulation_factor );

    out_color[pixel] = float4( out, 1.0f );
}

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
func cs_write_history( uint2 thread_id: SV_DispatchThreadID )->void
{
    uint2 pixel = thread_id.xy;

    out_color[pixel] = scene_color[pixel];
}


