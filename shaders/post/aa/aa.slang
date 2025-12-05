
layout( binding = 0, set = 0 ) Texture2D<float4> scene_color;
layout( binding = 1, set = 0 ) Texture2D<float4> scene_history;
layout( binding = 2, set = 0 ) RWTexture2D<float4> out_color;
layout( binding = 3, set = 0 ) SamplerState linear_sampler;

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
func cs_main( uint2 thread_id: SV_DispatchThreadID )->void
{
    uint2 pixel = thread_id.xy;

    float3 prev_color = scene_history[pixel].rgb;

    float2 uv = ( float2( pixel ) + 0.5f ) / float2( 1920.0f, 1080.0f );
    float3 curr_color = scene_color.SampleLevel( linear_sampler, uv, 0 ).rgb;

    float modulation_factor = 0.9f;
    float3 out = lerp( curr_color, prev_color, modulation_factor );

    out_color[pixel] = float4( out, 1.0f );
}

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
func cs_write_history( uint2 thread_id: SV_DispatchThreadID )->void
{
    uint2 pixel = thread_id.xy;

    out_color[pixel] = scene_color[pixel];
}


