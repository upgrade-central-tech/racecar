#include "../common.slang"

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_data;
layout( binding = 0, set = 1 ) Texture2D<float4> gbuffer_position;
layout( binding = 1, set = 1 ) Texture2D<float4> gbuffer_normal;
layout( binding = 2, set = 1 ) Texture2D<float2> layer_test_mask;
layout( binding = 3, set = 1 ) RWTexture2D<float4> out_color;
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
    float world_scale = 10.0f;
    float2 sample_uv = float2( world_position.x, world_position.z ) / world_scale;

    uint2 sample_pixel = uint2( uint( sample_uv.x ), uint( sample_uv.y ) );
    float2 layers = layer_test_mask[sample_pixel];

    out_color[pixel] = float4( layers, 0.0f, 1.0f );
}


