#include "bloom_data.slang"

layout( binding = 0, set = 0 ) Texture2D<float4> input;
layout( binding = 1, set = 0 ) RWTexture2D<float4> output;

layout( binding = 0, set = 1 ) ConstantBuffer<BloomData> settings;

layout( binding = 0, set = 2 ) SamplerState sampler;

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
func downsample( uint2 thread_id: SV_DispatchThreadID )->void
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

    uint input_w;
    uint input_h;
    input.GetDimensions( input_w, input_h );
    let delta = 1.f / float2( input_w, input_h );

    // Take 13 samples around the current texel (represented by "e")
    // a - b - c
    // - j - k -
    // d - e - f
    // - l - m -
    // g - h - i
    let a = input.Sample( sampler, float2( uv.x - 2.f * delta.x, uv.y + 2.f * delta.y ) ).rgb;
    let b = input.Sample( sampler, float2( uv.x, uv.y + 2 * delta.y ) ).rgb;
    let c = input.Sample( sampler, float2( uv.x + 2.f * delta.x, uv.y + 2 * delta.y ) ).rgb;

    let d = input.Sample( sampler, float2( uv.x - 2.f * delta.x, uv.y ) ).rgb;
    let e = input.Sample( sampler, uv ).rgb;
    let f = input.Sample( sampler, float2( uv.x + 2.f * delta.x, uv.y ) ).rgb;

    let g = input.Sample( sampler, float2( uv.x - 2.f * delta.x, uv.y - 2 * delta.y ) ).rgb;
    let h = input.Sample( sampler, float2( uv.x, uv.y - 2 * delta.y ) ).rgb;
    let i = input.Sample( sampler, float2( uv.x + 2 * delta.x, uv.y - 2 * delta.y ) ).rgb;

    let j = input.Sample( sampler, float2( uv.x - delta.x, uv.y + delta.y ) ).rgb;
    let k = input.Sample( sampler, float2( uv.x + delta.x, uv.y + delta.y ) ).rgb;
    let l = input.Sample( sampler, float2( uv.x - delta.x, uv.y - delta.y ) ).rgb;
    let m = input.Sample( sampler, float2( uv.x + delta.x, uv.y - delta.y ) ).rgb;

    var result = e * 0.125f;
    result += ( a + c + g + i ) * 0.03125f;
    result += ( b + d + f + h ) * 0.0625f;
    result += ( j + k + l + m ) * 0.125f;

    output[pixel] = float4( result, 1.f );
}


