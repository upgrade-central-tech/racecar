layout( binding = 0, set = 0 ) Texture2D<float4> scene_color;
layout( binding = 1, set = 0 ) RWTexture2D<float4> out_color;

[shader("compute")]
[numthreads(8, 8, 1)]
void cs_pp_test( uint2 thread_id: SV_DispatchThreadID ) {
    uint2 pixel = thread_id.xy;
    float3 flip = scene_color.Load( uint3( pixel, 0 ) ).rgb;
    out_color[pixel] = float4( 1.0f - flip, 1.0f );
}