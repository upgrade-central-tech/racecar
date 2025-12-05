#include "common.slang"
#include "gaussian.slang"

layout( binding = 0, set = 0 ) Texture2D<float4> input; ///< Horizontal blur result
layout( binding = 1, set = 0 ) RWTexture2D<float4> output; ///< Full Gaussian blur result

layout( binding = 0, set = 2 ) SamplerState sampler;

static const float delta = 1.f / 1080.f;

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
    let delta_y = 1.f / (float)height;

    var result = input.Sample( sampler, uv ).rgb * WEIGHTS[0];

    for ( var i = 1; i < BLUR_RADIUS; ++i ) {
        let up_offset = uv.y + ( (float)i * delta );
        let down_offset = uv.y - ( (float)i * delta );

        result += input.Sample( sampler, up_offset ).rgb * WEIGHTS[i];
        result += input.Sample( sampler, down_offset ).rgb * WEIGHTS[i];
    }

    output[pixel] = float4( result, 1.f );
}


