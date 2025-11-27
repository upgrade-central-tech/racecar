#include "../common.slang"

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_data;
layout( binding = 0, set = 1 ) Texture2D<float4> gbuffer_position;
layout( binding = 1, set = 1 ) Texture2D<float4> gbuffer_normal;
layout( binding = 2, set = 1 ) RWTexture2D<float4> out_color;
layout( binding = 0, set = 3 ) SamplerState nearest_sampler;

[shader( "compute" )]
[numthreads( 8, 8, 8 )]
void cs_terrain_draw( uint3 thread_id: SV_DispatchThreadID )
{
    uint2 pixel = thread_id.xy;

    if ( gbuffer_position[pixel].a != TERRAIN_ID ) {
        return;
    }

    float3 world_position = gbuffer_position[pixel].rgb;

    out_color[pixel] = float4( world_position, 1.0f );
}


