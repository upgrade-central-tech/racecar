#include "common.slang"
#include "gaussian.slang"

layout( binding = 0, set = 0 ) Texture2D<float4> input; ///< Brightness threshold
layout( binding = 1, set = 0 ) RWTexture2D<float4> output; ///< Horizontal blurred result

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
func cs_main( uint2 thread_id: SV_DispatchThreadID )->void
{
    if ( !debug.enable_bloom ) {
        return;
    }

    let pixel = thread_id.xy;

    uint width;
    uint height;
    input.GetDimensions( width, height );

    var result = input.Load( uint3( pixel, 0 ) ).rgb * WEIGHTS[0];

    for ( var i = 1; i < BLUR_RADIUS; ++i ) {
        let right_offset = min( pixel.x + i, width - 1 );
        let left_offset = max( pixel.x - i, 0 );

        result += input.Load( uint3( right_offset, pixel.y, 0 ) ).rgb * WEIGHTS[i];
        result += input.Load( uint3( left_offset, pixel.y, 0 ) ).rgb * WEIGHTS[i];
    }

    output[pixel] = float4( result, 1.f );
}


