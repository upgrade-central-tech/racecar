#include "../common.slang"
#include "../cloud_debug/cloud_utils.slang"

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_data;

layout( binding = 0, set = 1 ) Texture2D<float4> gbuffer_position;
layout( binding = 1, set = 1 ) Texture2D<float4> gbuffer_normal;
layout( binding = 2, set = 1 ) Texture2D<float2> layer_test_mask;
layout( binding = 3, set = 1 ) Texture2D<float4> grass_albedo;
layout( binding = 4, set = 1 ) RWTexture2D<float4> out_color;

layout( binding = 0, set = 3 ) SamplerState nearest_sampler;

// Compute Shader Filtering
float3 filter_texture() { return float3( 0.0f ); }

// High-frequency noise generator?

[shader( "compute" )]
[numthreads( 8, 8, 8 )]
void cs_terrain_draw( uint3 thread_id: SV_DispatchThreadID )
{
    uint2 pixel = thread_id.xy;

    if ( gbuffer_position[pixel].a != TERRAIN_ID ) {
        return;
    }

    float3 world_position = gbuffer_position[pixel].rgb;
    float world_scale = 0.125f;

    // UV Distortion to reduce layer-mask "blockiness" from texture sampling
    float distort_scale = 5.0f;
    float rand_sx = perlin_noise3D( distort_scale * world_position );
    float rand_sy = rand_sx; // perlin_noise3D( distort_scale * world_position );
    float distort_amplitude = 0.1f;
    float2 distortion_offset
        = 2.0f * float2( rand_sx, rand_sy ) * distort_amplitude - distort_amplitude;

    // Layer map texture
    float layer_map_scale = 0.05f;
    float2 uv = frac(
        ( distortion_offset + float2( world_position.x, world_position.z ) ) * layer_map_scale );
    float2 world_uv = frac( float2( world_position.x, world_position.z ) * world_scale );
    // uv = select( uv < 0.0f, uv + 1.0f, uv );

    // Our test image is 200x200.
    uint tex_size = 200;
    uint2 sample_pixel = uint2( uv * float2( tex_size, tex_size ) );

    // Naive-sample
    // So far:
    // R - Snow
    // G - Grass
    // B - Asphalt?
    float2 layers = layer_test_mask.Sample( nearest_sampler, uv );

    float3 grass = grass_albedo.Sample( nearest_sampler, world_uv ).rgb;
    float3 snow = float3( 1.0f, 1.0f, 1.0f );

    float3 final_color = lerp( float3( 0.0f ), snow, layers.x );
    final_color = lerp( final_color, grass, layers.y );

    out_color[pixel] = float4( final_color, 1.0f );
}


