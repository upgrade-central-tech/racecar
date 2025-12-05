layout( binding = 0, set = 0 ) Texture2D<float4> input;
layout( binding = 1, set = 0 ) RWTexture2D<float4> output;

layout( binding = 0, set = 1 ) SamplerState sampler;

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
func downsample( uint2 thread_id: SV_DispatchThreadID )->void
{
    let pixel = thread_id.xy;

    uint width;
    uint height;
    output.GetDimensions( width, height );
    let output_size = float2( width, height );

    if ( pixel.x >= width || pixel.y >= height ) {
        return;
    }

    let uv = ( float2( pixel ) + 0.5f ) / output_size;

    let result = input.Sample( sampler, uv );
    output[pixel] = result;
}


