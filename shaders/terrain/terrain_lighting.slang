#include "../common.slang"
#include "../utils.slang"
#include "../cloud_debug/cloud_utils.slang"

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_data;
layout( binding = 1, set = 0 ) ConstantBuffer<DebugData> scene_data;

layout( binding = 0, set = 1 ) Texture2D<float4> gbuffer_position;
layout( binding = 1, set = 1 ) Texture2D<float4> gbuffer_normal;
layout( binding = 2, set = 1 ) Texture2D<float2> layer_test_mask;
layout( binding = 3, set = 1 ) Texture2D<float4> grass_albedo_roughness;
layout( binding = 4, set = 1 ) Texture2D<float4> grass_normal_ao;

layout( binding = 5, set = 1 ) RWTexture2D<float4> out_color;

layout( binding = 0, set = 2 ) Texture2D<float4> sky;
layout( binding = 1, set = 2 ) Texture2D<float4> sky_irradiance;

layout( binding = 0, set = 3 ) SamplerState nearest_sampler;

// Misc.
float3 direct_lighting( float3 albedo, float3 normal, float roughness, float ao, float3 view )
{
    float3 light = normalize( scene_data.sun_direction.rgb );
    float3 half = normalize( view + light );

    BSDF_Dots dots = get_dots( normal, view, half, light );

    // Diffuse properties
    float3 diffuse_albedo = albedo / PI;

    // Specular properties
    float alpha = roughness * roughness;
    float NDF = DistributionGGX( dots.clamped_n_dot_h, alpha );
    float G = GeometrySmith( dots.clamped_n_dot_v, dots.clamped_n_dot_l, alpha );
    float3 F0 = float3( 0.04f );
    float3 F = FresnelSchlick( dots.clamped_n_dot_h, F0 );

    float3 specular
        = ( NDF * G * F ) / ( 4.0f * dots.clamped_n_dot_v * dots.clamped_n_dot_l + 0.001f );

    // CEDEC 22 GT7 Presentation
    float shadow_strength = 1.0f;
    float local_shadows
        = clamp( dot( normal, light ) - shadow_strength * ( 1.0f - ao ) + 0.5f, 0.0f, 1.0f );

    float clamped_lambert = clamp( dot( normal, light ), 0.0f, 1.0f );

    return ( ( 1.0f - F ) * diffuse_albedo + specular ) * clamped_lambert * local_shadows;
}

// Main
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

    // Misc. vectors
    float3 view = normalize( camera_data.camera_pos.rgb - world_position );

    //=======================================================================================
    // Material layer evaluation
    //=======================================================================================

    // UV Distortion to reduce layer-mask "blockiness" from texture sampling
    float distort_scale = 5.0f;
    float rand_sx = perlin_noise3D( distort_scale * world_position );
    float rand_sy = rand_sx; // perlin_noise3D( distort_scale * world_position );
    float distort_amplitude = 0.1f;
    float2 distortion_offset
        = 2.0f * float2( rand_sx, rand_sy ) * distort_amplitude - distort_amplitude;

    // Layer map texture
    float layer_map_scale = 0.05f;
    float world_scale = 0.05f;

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

    //=======================================================================================
    // Material property gather. TODO: use a struct?
    //=======================================================================================
    float4 grass_color_roughness = filter_float4( grass_albedo_roughness, world_uv );
    float4 grass_normal_map_ao = filter_float4( grass_normal_ao, world_uv );
    float3 grass_normal_map = grass_normal_map_ao.rgb;
    float3 grass_normal = map_normals( grass_normal_map, float4( 0, 0, 1, 1 ), normal );
    float3 grass_color = grass_color_roughness.rgb;

    // Bias towards the top.
    grass_normal = normalize( grass_normal + float3( 0.0f, 0.1f, 0.0f ) );

    float grass_roughness = grass_color_roughness.a;
    float grass_ao = grass_normal_map_ao.a;

    float3 snow_color = float3( 1.0f, 1.0f, 1.0f );
    float3 snow_normal = float3( 0.0f, 1.0f, 0.0f );
    float snow_roughness = 0.1f;
    float snow_ao = 1.0f;

    //=======================================================================================
    // Final material gather. These params get passed into the respective lighting
    // functions below.
    //=======================================================================================

    // Naive lerping??? What's a better solution?
    float3 mixed_albedo = lerp( float3( 0.0f ), snow_color, layers.x );
    mixed_albedo = lerp( mixed_albedo, grass_color, layers.y );

    // This is so obviously wrong, but we're going to try this out for now.
    float3 mixed_normal = lerp( float3( 0.0f, 1.0f, 0.0f ), snow_normal, layers.x );
    mixed_normal = lerp( mixed_normal, grass_normal, layers.y );

    // This is also so wrong.
    float mixed_ao = lerp( 1.0f, snow_ao, layers.x );
    mixed_ao = lerp( mixed_ao, grass_ao, layers.y );

    // ?? You know the drill
    float mixed_roughness = lerp( 1.0f, snow_roughness, layers.x );
    mixed_roughness = lerp( mixed_roughness, grass_roughness, layers.y );

    //=======================================================================================
    // Image-based lighting evaluations
    //=======================================================================================

    // Remap to [0-1]
    float2 oct_uv = oct_encode( mixed_normal );
    float2 oct_refl_uv = oct_encode( normalize( reflect( -view, mixed_normal ) ) );

    // Irradiance lighting type shizz
    uint irradiance_width, irradiance_height;
    sky_irradiance.GetDimensions( irradiance_width, irradiance_height );
    int2 irradiance_px = int2( oct_uv * float2( irradiance_width, irradiance_height ) );
    irradiance_px
        = clamp( irradiance_px, int2( 0, 0 ), int2( irradiance_width - 1, irradiance_height - 1 ) );

    float irradiance_strength = 1.0f;
    float3 diffuse_irradiance = filter_float4( sky_irradiance, oct_uv ).rgb;
    // sky_irradiance[irradiance_px].rgb;

    uint specular_width, specular_height;
    sky.GetDimensions( specular_width, specular_height );
    int2 sky_px = int2( oct_refl_uv * float2( specular_width, specular_height ) );
    sky_px = clamp( sky_px, int2( 0, 0 ), int2( specular_width - 1, specular_height - 1 ) );

    float specular_strength = 0.75f;
    float3 specular_irradiance
        = sky[sky_px].rgb; // ]filter_float4( sky, oct_refl_uv ).rgb; // [sky_px].rgb;

    //=======================================================================================
    // Final lighting evaluation
    //=======================================================================================
    float3 final_color
        = direct_lighting( mixed_albedo, mixed_normal, mixed_roughness, mixed_ao, view );

    final_color += ( mixed_albedo / PI ) * diffuse_irradiance * irradiance_strength;
    final_color += clamp( specular_irradiance, float3( 0.0f ), float3( 1.0f ) ) * 0.01f;

    out_color[pixel] = float4( final_color, 1.0f );
}


