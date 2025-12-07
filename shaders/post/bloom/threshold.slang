#include "bloom_data.slang"

layout( binding = 0, set = 0 ) Texture2D<float4> input;
layout( binding = 1, set = 0 ) RWTexture2D<float4> inout_texture;

layout( binding = 0, set = 1 ) ConstantBuffer<BloomData> settings;

/// See https://en.wikipedia.org/wiki/Grayscale or https://learnopengl.com/Advanced-Lighting/Bloom
static const float3 INTENSITY_COEFFICIENTS = { 0.2126f, 0.7152f, 0.0722f };

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
func threshold( uint2 thread_id: SV_DispatchThreadID )->void
{
    let pixel = thread_id.xy;
    let scene = input[pixel].rgb;

    let luminance = dot( scene, INTENSITY_COEFFICIENTS );
    let final = luminance > settings.threshold ? scene : float3( 0.f );

    inout_texture[pixel] = float4( final, 1.f );
}


