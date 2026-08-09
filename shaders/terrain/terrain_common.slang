#pragma once

#include "../common.slang"
#include "../utils.slang"
#include "../clouds/cloud_utils.slang"

static const float TERRAIN_DISTORT_SCALE = 5.0f;
static const float TERRAIN_DISTORT_AMPLITUDE = 0.1f;
static const float TERRAIN_LAYER_MAP_SCALE = 0.025f;
static const float TERRAIN_WORLD_SCALE = 0.05f;

static const float TERRAIN_HEIGHT_AMPLITUDE = 5.0f;
static const float TERRAIN_HEIGHT_FALLOFF_WIDTH = 50.0f;

static const float TERRAIN_PUDDLE_SCALE = 0.5f;
static const float TERRAIN_PUDDLE_SPEED = 13.333f;

struct TerrainMaps {
    Texture2D<float2> layer_mask;
    Texture2D<float4> grass_albedo_roughness;
    Texture2D<float4> grass_normal_ao;
    Texture2D<float4> asphalt_albedo_roughness;
    Texture2D<float4> asphalt_normal_ao;
    SamplerState linear_sampler;
};

struct TerrainGradients {
    float2 layer_ddx;
    float2 layer_ddy;
    float2 material_ddx;
    float2 material_ddy;
};

struct TerrainUVs {
    float2 layer_uv;
    float2 world_uv_unwrapped;
    float2 world_uv;
};

TerrainGradients terrain_no_gradients()
{
    TerrainGradients gradients;
    gradients.layer_ddx = float2( 0.0f );
    gradients.layer_ddy = float2( 0.0f );
    gradients.material_ddx = float2( 0.0f );
    gradients.material_ddy = float2( 0.0f );
    return gradients;
}

float2 terrain_layer_uv( float3 world_position, float2 scroll )
{
    float rand_s = perlin_noise3D(
        TERRAIN_DISTORT_SCALE * world_position + float3( scroll.x, 0.0f, scroll.y ) );

    float2 distortion_offset = 2.0f * float2( rand_s, rand_s ) * TERRAIN_DISTORT_AMPLITUDE
        - TERRAIN_DISTORT_AMPLITUDE;

    return ( distortion_offset + float2( world_position.x + 22, world_position.y + 60 ) )
        * TERRAIN_LAYER_MAP_SCALE
        + scroll;
}

float2 terrain_world_uv_unwrapped( float3 world_position, float2 scroll )
{
    return world_position.xz * TERRAIN_WORLD_SCALE + scroll;
}

TerrainUVs terrain_compute_uvs( float3 world_position, float2 scroll )
{
    TerrainUVs uvs;
    uvs.layer_uv = terrain_layer_uv( world_position, scroll );
    uvs.world_uv_unwrapped = terrain_world_uv_unwrapped( world_position, scroll );
    uvs.world_uv = uvs.world_uv_unwrapped - floor( uvs.world_uv_unwrapped );
    return uvs;
}

float terrain_height( float3 base_position, float2 scroll, Texture2D<float2> layer_mask,
    SamplerState linear_sampler )
{
    float2 layer_uv = terrain_layer_uv( base_position, scroll );
    float2 world_uv_unwrapped = terrain_world_uv_unwrapped( base_position, scroll );

    float2 layers = layer_mask.SampleLevel( linear_sampler, layer_uv, 0 );
    float layer = 1.0f - layers.r;

    float x_dist = clamp( abs( base_position.x / TERRAIN_HEIGHT_FALLOFF_WIDTH ), 0.0f, 1.0f );

    float ridge = perlin_noise3D(
                      float3( world_uv_unwrapped.x * 0.5f, 0.0f, world_uv_unwrapped.y * 0.5f ) )
            * 2.0f
        + 1.0f;

    return TERRAIN_HEIGHT_AMPLITUDE * x_dist * layer * ridge;
}

float3 terrain_displace( float3 base_position, float2 scroll, Texture2D<float2> layer_mask,
    SamplerState linear_sampler )
{
    float3 displaced = base_position;
    displaced.y = terrain_height( base_position, scroll, layer_mask, linear_sampler );
    return displaced;
}

float3 terrain_height_normal( float3 base_position, float2 scroll, Texture2D<float2> layer_mask,
    SamplerState linear_sampler, float epsilon )
{
    float3 dx = float3( epsilon, 0.0f, 0.0f );
    float3 dz = float3( 0.0f, 0.0f, epsilon );

    float height_x0 = terrain_height( base_position - dx, scroll, layer_mask, linear_sampler );
    float height_x1 = terrain_height( base_position + dx, scroll, layer_mask, linear_sampler );
    float height_z0 = terrain_height( base_position - dz, scroll, layer_mask, linear_sampler );
    float height_z1 = terrain_height( base_position + dz, scroll, layer_mask, linear_sampler );

    float3 tangent_x = float3( 2.0f * epsilon, height_x1 - height_x0, 0.0f );
    float3 tangent_z = float3( 0.0f, height_z1 - height_z0, 2.0f * epsilon );

    return normalize( cross( tangent_z, tangent_x ) );
}

/// https://advances.realtimerendering.com/other/2016/naughty_dog/NaughtyDog_TechArt_Final.pdf
void evaluate_wetness( float wetness, float porosity, inout float3 color, inout float roughness,
    inout float ao, inout float3 world_normal )
{
    float3 color_sq = color * color;
    color = lerp( color, color_sq, clamp_range( wetness, 0.1f, 0.35f ) * porosity );
    roughness = lerp( roughness, 0.1f, clamp_range( wetness, 0.2f, 1.0f ) );
    ao = lerp( ao, 1.0f, clamp_range( wetness, 0.45f, 0.95f ) );

    // GBuffer doesn't store the vertex normal, so we'll treat 0.45 case and 0.98 case identically.
    // Uncharted 4, however, uses frag normal for 0.45 and vert for 0.98.
    world_normal = normalize(
        lerp( world_normal, float3( 0.0f, 1.0f, 0.0f ), clamp_range( wetness, 0.45f, 1.0f ) ) );
}

struct TerrainSurface {
    float3 albedo;
    float3 normal;
    float roughness;
    float ao;

    float wetness;

    float snow_lerp;

    float2 layers;
};

TerrainSurface terrain_evaluate_surface( float3 world_position, float3 vertex_normal, float2 scroll,
    TerrainUVs uvs, float snowness, float wetness_toggle, TerrainMaps maps,
    TerrainGradients gradients )
{
    float2 layer_uv = uvs.layer_uv;
    float2 world_uv_unwrapped = uvs.world_uv_unwrapped;
    float2 world_uv = uvs.world_uv;

    float3 world_position_uvw = float3( world_position.x, 0.0f, world_position.z );

    // Snow
    float snow_map = perlin_noise3D(
                         0.075f * world_position_uvw + 0.23f + float3( scroll.x, 0.0f, scroll.y ) )
        + 0.50f;
    snow_map = lerp( clamp_range( snow_map, 0.50f, 0.80f ), 1.0f, snowness );

    // Distribute the wetness around da world...
    float3 puddle_uvw
        = TERRAIN_PUDDLE_SPEED * float3( world_uv_unwrapped.x, 0.0f, world_uv_unwrapped.y );

    float small_puddle_map = perlin_noise3D( TERRAIN_PUDDLE_SCALE * puddle_uvw ) + 0.25f;
    float big_puddle_map = perlin_noise3D( 0.125f * TERRAIN_PUDDLE_SCALE * puddle_uvw ) + 0.50f;

    float wetness = clamp_range( big_puddle_map, 0.35f, 0.75f )
        + clamp_range( small_puddle_map, 0.1f, 0.95f );

    // Material property gather
    // R - Asphalt
    // G - Grass/Terrain mask
    float2 layers = maps.layer_mask.SampleGrad(
        maps.linear_sampler, layer_uv, gradients.layer_ddx, gradients.layer_ddy );

    float4 grass_color_roughness = maps.grass_albedo_roughness.SampleGrad(
        maps.linear_sampler, world_uv, gradients.material_ddx, gradients.material_ddy );
    float4 grass_normal_map_ao = maps.grass_normal_ao.SampleGrad(
        maps.linear_sampler, world_uv, gradients.material_ddx, gradients.material_ddy );

    float3 grass_color = grass_color_roughness.rgb;
    float3 grass_normal
        = normalize( map_normals( grass_normal_map_ao.rgb, float4( 0, 0, 1, -1 ), vertex_normal ) );
    float grass_ao = grass_normal_map_ao.a;

    float4 asphalt_color_roughness = maps.asphalt_albedo_roughness.SampleGrad(
        maps.linear_sampler, world_uv, gradients.material_ddx, gradients.material_ddy );
    float4 asphalt_normal_map_ao = maps.asphalt_normal_ao.SampleGrad(
        maps.linear_sampler, world_uv, gradients.material_ddx, gradients.material_ddy );

    float3 asphalt_color = asphalt_color_roughness.rgb;
    float3 asphalt_normal = normalize(
        map_normals( asphalt_normal_map_ao.rgb, float4( 1, 0, 0, -1 ), vertex_normal ) );
    float asphalt_ao = asphalt_normal_map_ao.a;

    float asphalt_roughness = 1.0f - asphalt_color_roughness.a;
    float grass_roughness = 1.0f - grass_color_roughness.a;

    // Final material gather

    // Naive lerping??? What's a better solution?
    float3 mixed_albedo = lerp( float3( 0.7, 0.0, 0.0 ), asphalt_color, layers.x );
    float snow_lerp = snow_map * snowness * ( 1.0f - layers.x );

    grass_color = lerp( grass_color, float3( 1.0f ), snow_lerp );
    grass_ao = lerp( grass_ao, 1.0f, snow_lerp );

    mixed_albedo = lerp( mixed_albedo, grass_color, layers.y );

    // This is so obviously wrong, but we're going to try this out for now.
    float3 mixed_normal = lerp( float3( 0.0f, 1.0f, 0.0f ), asphalt_normal, layers.x );
    mixed_normal = lerp( mixed_normal, grass_normal, layers.y );

    // This is also so wrong.
    float mixed_ao = lerp( 1.0f, asphalt_ao, layers.x );
    mixed_ao = lerp( mixed_ao, grass_ao, layers.y );

    // ?? You know the drill
    float mixed_roughness = lerp( 1.0f, asphalt_roughness, layers.x );
    float ground_roughness = saturate( lerp( grass_roughness, 1.35f * grass_roughness, snow_lerp ) );

    mixed_roughness = lerp( mixed_roughness, ground_roughness, layers.y );

    // THE WORLD IS ALL WET.
    float mixed_wetness = lerp( 0.0f, wetness, wetness_toggle );
    float mixed_porosity = 0.0f;
    evaluate_wetness(
        mixed_wetness, mixed_porosity, mixed_albedo, mixed_roughness, mixed_ao, mixed_normal );

    TerrainSurface surface;
    surface.albedo = mixed_albedo;
    surface.normal = mixed_normal;
    surface.roughness = mixed_roughness;
    surface.ao = mixed_ao;
    surface.wetness = wetness;
    surface.snow_lerp = snow_lerp;
    surface.layers = layers;

    return surface;
}

void terrain_apply_wetness_exposure( float wetness_toggle, inout float3 albedo,
    inout float roughness )
{
    albedo = albedo * exp2( lerp( 0.0f, -1.5f, wetness_toggle ) );
    roughness = roughness * exp2( lerp( 0.0f, -1.5f, wetness_toggle ) );
}

static const float TERRAIN_INV_PI = 0.3183098861837906715377f;

float3 terrain_sample_glossy_ibl( float roughness, float2 direction, Texture2D<float4> sky_mips,
    SamplerState linear_sampler )
{
    float lod = roughness * 5.0f;
    uint lod0 = uint( floor( roughness * 5.0f ) );
    uint lod1 = min( lod0 + 1, 5 );
    float lod_t = lod - lod0;

    float3 glossy_lod0 = sky_mips.SampleLevel( linear_sampler, direction, lod0 ).rgb;
    float3 glossy_lod1 = sky_mips.SampleLevel( linear_sampler, direction, lod1 ).rgb;
    return lerp( glossy_lod0, glossy_lod1, lod_t );
}

float3 terrain_direct_lighting( float3 albedo, float3 normal, float3 light, float NDF,
    float roughness, float ao, float3 view, TerrainData terrain_data )
{
    float3 half_vector = normalize( view + light );
    BSDF_Dots dots = get_dots( normal, view, half_vector, light );

    // Diffuse properties
    var diffuse = albedo * TERRAIN_INV_PI;

    // Specular properties
    float alpha = roughness * roughness;

    float G = GeometrySmith( dots.clamped_n_dot_v, dots.clamped_n_dot_l, alpha );
    float3 F0 = float3( 0.04f );
    float3 F = FresnelSchlick( dots.clamped_n_dot_h, F0 );

    float3 specular
        = ( NDF * G * F ) / ( 4.0f * dots.clamped_n_dot_v * dots.clamped_n_dot_l + 0.001f );

    float shadowing = dot( normal, light );

    bool enable_gt7_ao = terrain_data.packed_floats0.x > 0.5f;
    if ( enable_gt7_ao ) {
        // CEDEC 22 GT7 Presentation
        float shadow_strength = terrain_data.terrain_data0.x;
        float local_shadows
            = clamp( dot( normal, light ) - shadow_strength * ( 1.0f - ao ) + 0.5f, 0.0f, 1.0f );
        shadowing *= local_shadows;
    } else {
        shadowing *= ao;
    }

    // Clamp shadows
    shadowing = clamp( shadowing, 0.0f, 1.0f );

    bool shadowing_only = terrain_data.packed_floats0.y > 0.5f;
    if ( shadowing_only ) {
        return shadowing;
    }

    return ( ( 1.0f - F ) * diffuse + specular ) * shadowing;
}
