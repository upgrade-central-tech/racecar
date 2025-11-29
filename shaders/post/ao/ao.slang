#include "../../common.slang"

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_data;

layout( binding = 0, set = 1 ) RWTexture2D<float4> output;
layout( binding = 1, set = 1 ) Texture2D<float4> scene_color;
layout( binding = 2, set = 1 ) Texture2D<float4> gbuffer_normal;
layout( binding = 3, set = 1 ) Texture2D<float4> gbuffer_depth;

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
func cs_ao( uint2 thread_id: SV_DispatchThreadID )->void
{
    uint2 pixel = thread_id.xy;

    float3 result = float3( 1.0f ) - scene_color[pixel].rgb;
    output[pixel] = float4( result, 1.f );
}


