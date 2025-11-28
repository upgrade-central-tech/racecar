#include "common.slang"

layout( binding = 0, set = 0 ) Texture2D<float4> blurred_color; ///< Final blurred texture
layout( binding = 1, set = 0 ) Texture2D<float4> scene_color; ///< Original scene color
layout( binding = 2, set = 0 ) RWTexture2D<float4> output; ///< Final gathered place

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
func cs_main( uint2 thread_id: SV_DispatchThreadID )->void
{
    let pixel = thread_id.xy;
    let scene = scene_color.Load( uint3( pixel, 0 ) ).rgb;

    var final : float3;
    if ( debug.enable_bloom ) {
        // Simple additive blending
        let blurred = blurred_color.Load( uint3( pixel, 0 ) ).rgb;
        final = scene + blurred;
    } else {
        final = scene;
    }

    output[pixel] = float4( final, 1.f );
}


