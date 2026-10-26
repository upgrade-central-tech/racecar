#pragma once

#include "../common.slang"
#include "../utils.slang"
#include "../clouds/cloud_utils.slang"

static const float TERRAIN_DISTORT_SCALE = 5.0f;
static const float TERRAIN_DISTORT_AMPLITUDE = 0.1f;
static const float TERRAIN_LAYER_MAP_SCALE = 0.025f;
static const float TERRAIN_WORLD_SCALE = 0.05f;

static const float TERRAIN_HEIGHT_AMPLITUDE = 2.0f;

static const float TERRAIN_TRIPLANAR_SHARPNESS = 4.0f;

static const float2x2 TERRAIN_GRASS_TANGENT_FRAME = float2x2( 0.0f, 1.0f, 1.0f, 0.0f );
static const float2x2 TERRAIN_ASPHALT_TANGENT_FRAME = float2x2( 1.0f, 0.0f, 0.0f, -1.0f );

static const float TERRAIN_PUDDLE_SCALE = 0.5f;
static const float TERRAIN_PUDDLE_SPEED = 13.333f;
static const float TERRAIN_SLOPE_MAX_WETNESS = 0.4f;

struct TerrainMaps {
    Texture2D<float4> grass_albedo_roughness;
    Texture2D<float4> grass_normal_ao;
    Texture2D<float4> asphalt_albedo_roughness;
    Texture2D<float4> asphalt_normal_ao;
    SamplerState linear_sampler;
};

struct TerrainGradients {
    float2 layer_ddx;
    float2 layer_ddy;
    float3 position_ddx;
    float3 position_ddy;
};

struct TerrainUVs {
    float2 layer_uv;
    float2 world_uv_unwrapped;
};

TerrainGradients terrain_no_gradients()
{
    TerrainGradients gradients;
    gradients.layer_ddx = float2( 0.0f );
    gradients.layer_ddy = float2( 0.0f );
    gradients.position_ddx = float3( 0.0f );
    gradients.position_ddy = float3( 0.0f );
    return gradients;
}

float2 terrain_layer_uv( float3 world_position, float2 scroll )
{
    float rand_s = perlin_noise3D(
        TERRAIN_DISTORT_SCALE * float3( world_position.x, 0.0f, world_position.z )
        + float3( scroll.x, 0.0f, scroll.y ) * ( TERRAIN_DISTORT_SCALE / TERRAIN_WORLD_SCALE ) );

    float2 distortion_offset = 2.0f * float2( rand_s, rand_s ) * TERRAIN_DISTORT_AMPLITUDE
        - TERRAIN_DISTORT_AMPLITUDE;

    return ( distortion_offset + float2( world_position.x + 22, world_position.z + 60 ) )
        * TERRAIN_LAYER_MAP_SCALE
        + scroll * ( TERRAIN_LAYER_MAP_SCALE / TERRAIN_WORLD_SCALE );
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
    return uvs;
}

struct TerrainTriplanarPlane {
    float2 uv;
    float2 uv_ddx;
    float2 uv_ddy;
    float3 u_axis;
    float3 v_axis;
};

struct TerrainTriplanar {
    TerrainTriplanarPlane planes[3];
    float3 weights;
};

TerrainTriplanarPlane terrain_triplanar_plane( float2 uv_unwrapped, float2 uv_ddx, float2 uv_ddy,
    float3 u_axis, float3 v_axis )
{
    TerrainTriplanarPlane plane;
    plane.uv = uv_unwrapped - floor( uv_unwrapped );
    plane.uv_ddx = uv_ddx;
    plane.uv_ddy = uv_ddy;
    plane.u_axis = u_axis;
    plane.v_axis = v_axis;
    return plane;
}

TerrainTriplanar terrain_compute_triplanar(
    float3 position, float3 surface_normal, float2 scroll, TerrainGradients gradients )
{
    float3 p = position * TERRAIN_WORLD_SCALE;
    float3 p_ddx = gradients.position_ddx * TERRAIN_WORLD_SCALE;
    float3 p_ddy = gradients.position_ddy * TERRAIN_WORLD_SCALE;

    TerrainTriplanar triplanar;
    triplanar.planes[0] = terrain_triplanar_plane( float2( p.z + scroll.y, p.y ), p_ddx.zy,
        p_ddy.zy, float3( 0.0f, 0.0f, 1.0f ), float3( 0.0f, 1.0f, 0.0f ) );
    triplanar.planes[1] = terrain_triplanar_plane( p.xz + scroll, p_ddx.xz, p_ddy.xz,
        float3( 1.0f, 0.0f, 0.0f ), float3( 0.0f, 0.0f, 1.0f ) );
    triplanar.planes[2] = terrain_triplanar_plane( float2( p.x + scroll.x, p.y ), p_ddx.xy,
        p_ddy.xy, float3( 1.0f, 0.0f, 0.0f ), float3( 0.0f, 1.0f, 0.0f ) );

    float3 weights = pow( abs( surface_normal ), TERRAIN_TRIPLANAR_SHARPNESS );
    triplanar.weights = weights / ( weights.x + weights.y + weights.z );
    return triplanar;
}

float4 terrain_triplanar_sample(
    Texture2D<float4> map, SamplerState linear_sampler, TerrainTriplanar triplanar )
{
    float4 result = float4( 0.0f );
    [unroll]
    for ( int i = 0; i < 3; i++ ) {
        TerrainTriplanarPlane plane = triplanar.planes[i];
        result += triplanar.weights[i]
            * map.SampleGrad( linear_sampler, plane.uv, plane.uv_ddx, plane.uv_ddy );
    }
    return result;
}

float4 terrain_triplanar_sample_normal_ao( Texture2D<float4> map, SamplerState linear_sampler,
    TerrainTriplanar triplanar, float3 surface_normal, float2x2 tangent_frame )
{
    float3 normal = normalize( surface_normal );
    float3 blended_normal = float3( 0.0f );
    float ao = 0.0f;

    [unroll]
    for ( int i = 0; i < 3; i++ ) {
        TerrainTriplanarPlane plane = triplanar.planes[i];
        float4 normal_map_ao
            = map.SampleGrad( linear_sampler, plane.uv, plane.uv_ddx, plane.uv_ddy );

        float3 tangent = tangent_frame[0].x * plane.u_axis + tangent_frame[0].y * plane.v_axis;
        float3 bitangent = tangent_frame[1].x * plane.u_axis + tangent_frame[1].y * plane.v_axis;
        float3 normal_tangent = 2.0f * normal_map_ao.rgb - 1.0f;

        float3 plane_normal = normalize(
            tangent * normal_tangent.x + bitangent * normal_tangent.y + normal * normal_tangent.z );

        blended_normal += triplanar.weights[i] * plane_normal;
        ao += triplanar.weights[i] * normal_map_ao.a;
    }

    return float4( normalize( blended_normal ), ao );
}

float dist_sq_to_segment( float2 a, float2 b )
{
    float2 ab = b - a;
    float t = saturate( -dot( a, ab ) / max( dot( ab, ab ), 1e-8f ) );
    float2 closest = a + t * ab;
    return dot( closest, closest );
}

// https://iquilezles.org/articles/distfunctions2d/
float dist_sq_to_quadratic_bezier( float2 A, float2 B, float2 C )
{
    float2 a = B - A;
    float2 b = A - 2.0f * B + C;
    float2 c = 2.0f * a;
    float2 d = A;

    float bb = dot( b, b );
    if ( bb < 1e-6f * max( dot( a, a ), 1e-8f ) ) {
        return dist_sq_to_segment( A, C );
    }

    float kk = 1.0f / bb;
    float kx = kk * dot( a, b );
    float ky = kk * ( 2.0f * dot( a, a ) + dot( d, b ) ) / 3.0f;
    float kz = kk * dot( d, a );

    float p = ky - kx * kx;
    float q = kx * ( 2.0f * kx * kx - 3.0f * ky ) + kz;
    float h = q * q + 4.0f * p * p * p;

    if ( h >= 0.0f ) {
        h = sqrt( h );
        float2 x = ( float2( h, -h ) - q ) / 2.0f;
        float2 uv = sign( x ) * pow( abs( x ), float2( 1.0f / 3.0f ) );
        float t = saturate( uv.x + uv.y - kx );
        float2 q0 = d + ( c + b * t ) * t;
        return dot( q0, q0 );
    }

    float z = sqrt( -p );
    float v = acos( clamp( q / ( p * z * 2.0f ), -1.0f, 1.0f ) ) / 3.0f;
    float m = cos( v );
    float n = sin( v ) * 1.732050808f;
    float2 t = saturate( float2( m + m, -n - m ) * z - kx );
    float2 q0 = d + ( c + b * t.x ) * t.x;
    float2 q1 = d + ( c + b * t.y ) * t.y;
    return min( dot( q0, q0 ), dot( q1, q1 ) );
}

float dist_to_spline( float3 position, float3[ROAD_NUM_POINTS] spline_positions, float3 world_offset ) {
    float2 p = position.xz + world_offset.xz;

    float min_dist_sq = 1e30f;
    for ( int i = 0; i < ROAD_NUM_POINTS - 2; i++ ) {
        float2 p0 = spline_positions[i].xz - p;
        float2 p1 = spline_positions[i + 1].xz - p;
        float2 p2 = spline_positions[i + 2].xz - p;

        min_dist_sq = min(
            min_dist_sq, dist_sq_to_quadratic_bezier( 0.5f * ( p0 + p1 ), p1, 0.5f * ( p1 + p2 ) )
        );
    }
    return sqrt( min_dist_sq );
}

float terrain_height( float3 base_position, float2 scroll, float3[ROAD_NUM_POINTS] spline_positions,
    SamplerState linear_sampler, float3 world_position, float road_width, float blend_width )
{
    float2 layer_uv = terrain_layer_uv( base_position, scroll );
    float2 world_uv_unwrapped = terrain_world_uv_unwrapped( base_position, scroll );

    float dist = dist_to_spline( base_position, spline_positions, world_position );
    float layer = saturate( smoothstep( road_width, road_width + blend_width, dist ) );

    float ridge = perlin_noise3D(
                      float3( world_uv_unwrapped.x * 0.5f, 0.0f, world_uv_unwrapped.y * 0.5f ) )
            * 2.0f
        + 1.0f;

    return TERRAIN_HEIGHT_AMPLITUDE * layer * ridge;
}

float3 terrain_displace( float3 base_position, float3 world_position, float2 scroll, float3[ROAD_NUM_POINTS] spline_positions,
    SamplerState linear_sampler, float road_width, float blend_width )
{
    float3 displaced = base_position;
    displaced.y = terrain_height( base_position, scroll, spline_positions, linear_sampler, world_position, road_width, blend_width );
    return displaced;
}

float terrain_tessellation_weight( float3 base_position, float3 world_position, float3[ROAD_NUM_POINTS] spline_positions, float road_width, float blend_width, float padding )
{
    float dist = dist_to_spline( base_position, spline_positions, world_position );
    return ( 1.0f - saturate( smoothstep( road_width + blend_width * 0.5f + padding, road_width + blend_width + padding, dist ) ) ) * step( road_width - padding, dist );
}

float3 terrain_height_normal( float3 base_position, float3 world_position, float2 scroll, float3[ROAD_NUM_POINTS] spline_positions,
    SamplerState linear_sampler, float epsilon, float road_width, float blend_width )
{
    float3 dx = float3( epsilon, 0.0f, 0.0f );
    float3 dz = float3( 0.0f, 0.0f, epsilon );

    float height_x0 = terrain_height( base_position - dx, scroll, spline_positions, linear_sampler, world_position, road_width, blend_width );
    float height_x1 = terrain_height( base_position + dx, scroll, spline_positions, linear_sampler, world_position, road_width, blend_width );
    float height_z0 = terrain_height( base_position - dz, scroll, spline_positions, linear_sampler, world_position, road_width, blend_width );
    float height_z1 = terrain_height( base_position + dz, scroll, spline_positions, linear_sampler, world_position, road_width, blend_width );

    float3 tangent_x = float3( 2.0f * epsilon, height_x1 - height_x0, 0.0f );
    float3 tangent_z = float3( 0.0f, height_z1 - height_z0, 2.0f * epsilon );

    return normalize( cross( tangent_z, tangent_x ) );
}

/// https://advances.realtimerendering.com/other/2016/naughty_dog/NaughtyDog_TechArt_Final.pdf
void evaluate_wetness( float wetness, float porosity, float3 vertex_normal, inout float3 color,
    inout float roughness, inout float ao, inout float3 world_normal )
{
    float3 color_sq = color * color;
    color = lerp( color, color_sq, clamp_range( wetness, 0.1f, 0.35f ) * porosity );
    roughness = lerp( roughness, 0.1f, clamp_range( wetness, 0.2f, 1.0f ) );
    ao = lerp( ao, 1.0f, clamp_range( wetness, 0.45f, 0.95f ) );

    // GBuffer doesn't store the vertex normal, so we'll treat 0.45 case and 0.98 case identically.
    // Uncharted 4, however, uses frag normal for 0.45 and vert for 0.98.
    world_normal = normalize(
        lerp( world_normal, normalize( vertex_normal ), clamp_range( wetness, 0.45f, 1.0f ) ) );
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

TerrainSurface terrain_evaluate_surface( float3 position, float3 vertex_normal, float2 scroll,
    TerrainUVs uvs, float snowness, float wetness_toggle, TerrainMaps maps,
    TerrainGradients gradients, float3[ROAD_NUM_POINTS] spline_positions, float4 gamestate_world_position, float road_width, float blend_width )
{
    float2 layer_uv = uvs.layer_uv;
    float2 world_uv_unwrapped = uvs.world_uv_unwrapped;

    float3 world_uvw_unwrapped = float3( world_uv_unwrapped.x, 0.0f, world_uv_unwrapped.y );

    // Snow
    float snow_map
        = perlin_noise3D( ( 0.075f / TERRAIN_WORLD_SCALE ) * world_uvw_unwrapped + 0.23f ) + 0.50f;
    snow_map = lerp( clamp_range( snow_map, 0.50f, 0.80f ), 1.0f, snowness );

    // Distribute the wetness around da world...
    float3 puddle_uvw
        = TERRAIN_PUDDLE_SPEED * float3( world_uv_unwrapped.x, 0.0f, world_uv_unwrapped.y );

    float small_puddle_map = perlin_noise3D( TERRAIN_PUDDLE_SCALE * puddle_uvw ) + 0.25f;
    float big_puddle_map = perlin_noise3D( 0.125f * TERRAIN_PUDDLE_SCALE * puddle_uvw ) + 0.50f;

    float wetness = clamp_range( big_puddle_map, 0.35f, 0.75f )
        + clamp_range( small_puddle_map, 0.1f, 0.95f );
    float flatness = smoothstep( 0.9f, 0.98f, normalize( vertex_normal ).y );
    wetness = min( wetness, lerp( TERRAIN_SLOPE_MAX_WETNESS, 2.0f, flatness ) );

    // Material property gather
    // R - Asphalt
    // G - Grass/Terrain mask
    float road_dist = dist_to_spline( position, spline_positions, gamestate_world_position.xyz );
    float layer = saturate( smoothstep( road_width, road_width + blend_width, road_dist ) );
    float2 layers = float2( 1.0 - layer, layer );

    TerrainTriplanar triplanar
        = terrain_compute_triplanar( position, normalize( vertex_normal ), scroll, gradients );

    float4 grass_color_roughness
        = terrain_triplanar_sample( maps.grass_albedo_roughness, maps.linear_sampler, triplanar );
    float4 grass_normal_and_ao = terrain_triplanar_sample_normal_ao( maps.grass_normal_ao,
        maps.linear_sampler, triplanar, vertex_normal, TERRAIN_GRASS_TANGENT_FRAME );

    float3 grass_color = grass_color_roughness.rgb;
    float3 grass_normal = grass_normal_and_ao.xyz;
    float grass_ao = grass_normal_and_ao.w;

    float4 asphalt_color_roughness = terrain_triplanar_sample(
        maps.asphalt_albedo_roughness, maps.linear_sampler, triplanar );
    float4 asphalt_normal_and_ao = terrain_triplanar_sample_normal_ao( maps.asphalt_normal_ao,
        maps.linear_sampler, triplanar, vertex_normal, TERRAIN_ASPHALT_TANGENT_FRAME );

    float3 asphalt_color = asphalt_color_roughness.rgb;
    float3 asphalt_normal = asphalt_normal_and_ao.xyz;
    float asphalt_ao = asphalt_normal_and_ao.w;

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
    evaluate_wetness( mixed_wetness, mixed_porosity, vertex_normal, mixed_albedo, mixed_roughness,
        mixed_ao, mixed_normal );

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
    float lod = roughness * SKY_MAX_MIP;
    uint lod0 = uint( floor( lod ) );
    uint lod1 = min( lod0 + 1, uint( SKY_MAX_MIP ) );
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
    float3 F = FresnelSchlick( dots.clamped_v_dot_h, F0 );

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

    return ( ( 1.0f - F ) * diffuse + specular ) * SUN_IRRADIANCE * shadowing;
}
