#include "common.slang"
#include "gaussian.slang"

layout( binding = 0, set = 0 ) Texture2D<float4> input; ///< Brightness threshold
layout( binding = 1, set = 0 ) RWTexture2D<float4> output; ///< Horizontal blurred result

layout( binding = 0, set = 2 ) SamplerState sampler;

static const float delta = 1.f / 1920.f;

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

    let uv = float2( (float)pixel.x / (float)width, (float)pixel.y / (float)height );
    let delta_x = 1.f / (float)width;

    var result = input.Sample( sampler, uv ).rgb * WEIGHTS[0];

    for ( var i = 1; i < BLUR_RADIUS; ++i ) {
        let right_offset = uv.x + ( (float)i * delta );
        let left_offset = uv.x - ( (float)i * delta );

        result += input.Sample( sampler, right_offset ).rgb * WEIGHTS[i];
        result += input.Sample( sampler, left_offset ).rgb * WEIGHTS[i];
    }

    output[pixel] = float4( result, 1.f );
}


