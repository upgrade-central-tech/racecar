#include "../common.slang"
#include "../utils.slang"
#include "../cloud_debug/cloud_utils.slang"

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_data;
layout( binding = 1, set = 0 ) ConstantBuffer<DebugData> scene_data;

layout( binding = 0, set = 1 ) Texture2D<float4> gbuffer_position;
layout( binding = 1, set = 1 ) Texture2D<float4> gbuffer_normal;

layout( binding = 2, set = 1 ) Texture2D<float2> layer_test_mask;
layout( binding = 3, set = 1 ) Texture2D<float4> grass_albedo;
layout( binding = 4, set = 1 ) Texture2D<float4> grass_normal_ao;

layout( binding = 5, set = 1 ) RWTexture2D<float4> out_color;

layout( binding = 0, set = 3 ) SamplerState nearest_sampler;

// Compute Shader Filtering
float3 filter_texture() { return float3( 0.0f ); }

// High-frequency noise generator?

float3 lighting( float3 albedo, float3 normal, float ao, float3 view )
{
    float3 light_dir = normalize( scene_data.sun_direction.rgb );
    float lambert = dot( normal, light_dir );
    return albedo * lambert * ao;
}

[shader( "compute" )]
[numthreads( 8, 8, 8 )]
void cs_terrain_draw( uint3 thread_id: SV_DispatchThreadID )
{
    uint2 pixel = thread_id.xy;

    if ( gbuffer_position[pixel].a != TERRAIN_ID ) {
        return;
    }

    float3 normal = gbuffer_normal[pixel].rgb;
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

    float3 grass_color = grass_albedo.Sample( nearest_sampler, world_uv ).rgb;
    float4 grass_normal_map_ao = grass_normal_ao.Sample( nearest_sampler, world_uv );
    float3 grass_normal_map = grass_normal_map_ao.rgb;
    float3 grass_normal = map_normals( grass_normal_map, float4( 0, 0, 1, -1 ), normal );
    float grass_ao = grass_normal_map_ao.a;

    float3 snow_color = float3( 1.0f, 1.0f, 1.0f );
    float3 snow_normal = float3( 0.0f, 1.0f, 0.0f );
    float snow_ao = 1.0f;

    // Naive lerping??? What's a better solution?
    float3 mixed_albedo = lerp( float3( 0.0f ), snow_color, layers.x );
    mixed_albedo = lerp( mixed_albedo, grass_color, layers.y );

    // This is so obviously wrong, but we're going to try this out for now.
    float3 mixed_normal = lerp( float3( 0.0f, 1.0f, 0.0f ), snow_normal, layers.x );
    mixed_normal = lerp( mixed_normal, grass_normal, layers.y );

    // This is also so wrong.
    float mixed_ao = lerp( 1.0f, snow_ao, layers.x );
    mixed_ao = lerp( mixed_ao, grass_ao, layers.y );

    float3 final_color = lighting( mixed_albedo, mixed_normal, mixed_ao, float3( 0.0f ) );

    out_color[pixel] = float4( final_color, 1.0f );
}


