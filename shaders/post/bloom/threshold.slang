#include "common.slang"

layout( binding = 0, set = 0 ) Texture2D<float4> scene_color;
layout( binding = 1, set = 0 ) RWTexture2D<float4> out_color;

/// See https://en.wikipedia.org/wiki/Grayscale or https://learnopengl.com/Advanced-Lighting/Bloom
static const float3 INTENSITY_COEFFICIENTS = { 0.2126f, 0.7152f, 0.0722f };

/// Since we're using HDR rendering, radiance values can be greater than 1
static const float BRIGHTNESS_THRESHOLD = 1.f;

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
func threshold( uint2 thread_id: SV_DispatchThreadID )->void
{
    let pixel = thread_id.xy;
    let scene = scene_color.Load( uint3( pixel, 0 ) ).rgb;

    let luminance = dot( scene, INTENSITY_COEFFICIENTS );

    let final = luminance > BRIGHTNESS_THRESHOLD ? scene : float3( 0.f );
    out_color[pixel] = float4( final, 1.f );
}


