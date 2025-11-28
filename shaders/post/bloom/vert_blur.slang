#include "common.slang"
#include "gaussian.slang"

layout( binding = 0, set = 0 ) Texture2D<float4> input; ///< Horizontal blur result
layout( binding = 1, set = 0 ) RWTexture2D<float4> output; ///< Full Gaussian blur result

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
    result = min( MAX_RADIANCE, result );

    for ( var i = 1; i < BLUR_RADIUS; ++i ) {
        let up_offset = min( pixel.y + i, height - 1 );
        let down_offset = max( pixel.y - i, 0 );

        result
            += min( MAX_RADIANCE, input.Load( uint3( pixel.x, up_offset, 0 ) ).rgb * WEIGHTS[i] );
        result
            += min( MAX_RADIANCE, input.Load( uint3( pixel.x, down_offset, 0 ) ).rgb * WEIGHTS[i] );
    }

    output[pixel] = float4( result, 1.f );
}


