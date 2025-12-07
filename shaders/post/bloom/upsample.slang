#include "bloom_data.slang"

layout( binding = 0, set = 0 ) Texture2D<float4> input;
layout( binding = 1, set = 0 ) RWTexture2D<float4> output;

layout( binding = 0, set = 1 ) ConstantBuffer<BloomData> settings;

layout( binding = 0, set = 2 ) SamplerState sampler;

static const float ONE_SIXTEENTH = 1.f / 16.f;

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
func upsample( uint2 thread_id: SV_DispatchThreadID )->void
{
    if ( !settings.enable ) {
        return;
    }

    let pixel = thread_id.xy;

    uint output_w;
    uint output_h;
    output.GetDimensions( output_w, output_h );

    if ( pixel.x >= output_w || pixel.y >= output_h ) {
        return;
    }

    let uv = float2( pixel ) / float2( output_w, output_h );
    let delta = float2( settings.filter_radius );

    // Take 9 samples around the current texel (represented by "e")
    // a - b - c
    // d - e - f
    // g - h - i
    let a = input.Sample( sampler, float2( uv.x - delta.x, uv.y + delta.y ) ).rgb;
    let b = input.Sample( sampler, float2( uv.x, uv.y + delta.y ) ).rgb;
    let c = input.Sample( sampler, float2( uv.x + delta.x, uv.y + delta.y ) ).rgb;

    let d = input.Sample( sampler, float2( uv.x - delta.x, uv.y ) ).rgb;
    let e = input.Sample( sampler, float2( uv.x, uv.y ) ).rgb;
    let f = input.Sample( sampler, float2( uv.x + delta.x, uv.y ) ).rgb;

    let g = input.Sample( sampler, float2( uv.x - delta.x, uv.y - delta.y ) ).rgb;
    let h = input.Sample( sampler, float2( uv.x, uv.y - delta.y ) ).rgb;
    let i = input.Sample( sampler, float2( uv.x + delta.x, uv.y - delta.y ) ).rgb;

    // Apply 3x3 tent filter
    //        | 1 2 1 |
    // 1/16 * | 2 4 2 |
    //        | 1 2 1 |
    var result = e * 4.f;
    result += ( b + d + f + h ) * 2.f;
    result += ( a + c + g + i );
    result *= ONE_SIXTEENTH;

    output[pixel] = float4( output.Load( pixel ).rgb + result, 1.f );
}


