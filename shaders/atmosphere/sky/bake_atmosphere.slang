#define HARDCODED_OCTAHEDRAL_SIZE 512.0f
#define HARDCODED_IRRADIANCE_SIZE 32.0f
#define COMPUTE_NO_SAMPLE 1

struct OctahedralData {
    // Pack: mip level
    float4 packedfloats0;
}

import atmosphere;
import bindings;
import constants;
import "../../clouds/cheap_raymarch.slang";

#include "../atmosphere/geo/atms_geo.slang"

// In atmosphere, the bindings are already there and used for other things.
layout( binding = 0, set = 3 ) RWTexture2D<float4> octahedral_sky;
// Octahedral sky used for irradiance
layout( binding = 1, set = 3 ) RWTexture2D<float4> octahedral_sky_irradiance;
layout( binding = 2, set = 3 ) RWTexture2D<float4> octahedral_sky_mip;
layout( binding = 3, set = 3 ) ConstantBuffer<OctahedralData> octahedral_data;

layout( binding = 0, set = 4 ) Texture2D<float> cumulus_map_LUT;
layout( binding = 1, set = 4 ) Texture3D<float> low_freq_noise_LUT;
layout( binding = 2, set = 4 ) SamplerState linear_mirrored_repeat_sampler;

float3 oct_decode( float2 f )
{
    // Map from [0, 1] to [-1, 1]
    f = f * 2.0f - 1.0f;

    float3 n = float3( f.x, f.y, 1.0f - abs( f.x ) - abs( f.y ) );

    // Unfold the lower hemisphere
    if ( n.z < 0.0f )
        n.xy = ( 1.0f - abs( n.yx ) ) * sign( n.xy );

    return normalize( n );
}

float2 oct_encode( float3 n )
{
    // Project the normal onto the octahedron
    n /= ( abs( n.x ) + abs( n.y ) + abs( n.z ) );

    // Fold the lower hemisphere
    if ( n.z < 0.0f )
        n.xy = ( 1.0f - abs( n.yx ) ) * sign( n.xy );

    // Map from [-1, 1] to [0, 1]
    return n.xy * 0.5f + 0.5f;
}

float RadicalInverse_VdC( uint bits )
{
    bits = ( bits << 16u ) | ( bits >> 16u );
    bits = ( ( bits & 0x55555555u ) << 1u ) | ( ( bits & 0xAAAAAAAAu ) >> 1u );
    bits = ( ( bits & 0x33333333u ) << 2u ) | ( ( bits & 0xCCCCCCCCu ) >> 2u );
    bits = ( ( bits & 0x0F0F0F0Fu ) << 4u ) | ( ( bits & 0xF0F0F0F0u ) >> 4u );
    bits = ( ( bits & 0x00FF00FFu ) << 8u ) | ( ( bits & 0xFF00FF00u ) >> 8u );
    return float( bits ) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley( uint i, uint N )
{
    return float2( float( i ) / float( N ), RadicalInverse_VdC( i ) );
}

float3 ImportanceSampleGGX( float2 Xi, float3 N, float roughness )
{
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt( ( 1.0 - Xi.y ) / ( 1.0 + ( a * a - 1.0 ) * Xi.y ) );
    float sinTheta = sqrt( 1.0 - cosTheta * cosTheta );

    // from spherical coordinates to cartesian coordinates
    float3 H;
    H.x = cos( phi ) * sinTheta;
    H.y = sin( phi ) * sinTheta;
    H.z = cosTheta;

    // from tangent-space vector to world-space sample vector
    float3 up = select( abs( N.z ) < 0.999, float3( 0.0, 0.0, 1.0 ), float3( 1.0, 0.0, 0.0 ) );
    float3 tangent = normalize( cross( up, N ) );
    float3 bitangent = cross( N, tangent );

    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize( sampleVec );
}

[shader( "compute" )]
[numthreads( 8, 8, 8 )]
void cs_bake_atmosphere( uint3 thread_id: SV_DispatchThreadID )
{
    // Thread XY represents a direction.
    uint2 pixel = thread_id.xy;
    uint face = thread_id.z;

    float u = ( float( pixel.x ) + 0.5f ) / HARDCODED_OCTAHEDRAL_SIZE;
    float v = ( float( pixel.y ) + 0.5f ) / HARDCODED_OCTAHEDRAL_SIZE;

    float3 view_direction = oct_decode( float2( u, v ) );

    // Insert the work to actually derive a 3D global direction from pixel and face
    float3 sky_color = get_sky_color( view_direction, 0.0001f );

    float3 origin = float3( 0.0f, 10.0f, 0.0f );

    float4 clouds = raymarch( cumulus_map_LUT, low_freq_noise_LUT, linear_mirrored_repeat_sampler,
        view_direction, origin, get_sun_direction() );

    sky_color = lerp( sky_color, clouds.rgb, clouds.a );

    octahedral_sky[pixel] = float4( sky_color, 1.0f );
}

[shader( "compute" )]
[numthreads( 8, 8, 8 )]
void cs_bake_atmosphere_irradiance( uint3 thread_id: SV_DispatchThreadID )
{
    // Thread XY represents a direction.
    uint2 pixel = thread_id.xy;
    uint face = thread_id.z;

    float u = ( float( pixel.x ) + 0.5f ) / HARDCODED_IRRADIANCE_SIZE;
    float v = ( float( pixel.y ) + 0.5f ) / HARDCODED_IRRADIANCE_SIZE;

    float3 view_direction = oct_decode( float2( u, v ) );

    float3 normal = normalize( view_direction );
    float3 up = float3( 0.0f, 1.0f, 0.0f );
    float3 right = normalize( cross( up, normal ) );
    up = normalize( cross( normal, right ) );

    float3 light = normalize( uniforms.sun_direction );

    let atms_world_position = float3( 0.0f );
    let sun_sky = get_sun_and_sky_irradiance( irradiance_lut, transmittance_lut, sampler,
        atms_world_position - EARTH_CENTER, normal, light );

    var ground = ( sun_sky._0 + sun_sky._1 );

    let atms_camera_pos = float3( 0.0f );
    let scatter = get_sky_radiance_to_point( transmittance_lut, scattering_lut, sampler,
        atms_camera_pos - EARTH_CENTER, atms_world_position - EARTH_CENTER, light, true );

    float3 irradiance = ground * scatter._1 + scatter._0;

    octahedral_sky_irradiance[pixel] = float4( irradiance, 1.0f );
}

[shader( "compute" )]
[numthreads( 8, 8, 8 )]
void cs_bake_atmosphere_mips( uint3 thread_id: SV_DispatchThreadID )
{
    uint2 pixel = thread_id.xy;
    uint mip = uint( octahedral_data.packedfloats0.x );
    uint size = 512 >> mip;

    float u = ( float( pixel.x ) + 0.5f ) / float( size );
    float v = ( float( pixel.y ) + 0.5f ) / float( size );

    float3 view_direction = oct_decode( float2( u, v ) );

    // Split-sum assumption
    float3 normal = normalize( view_direction );
    float3 reflect = normal;
    float3 view = normal;

    // If we're at mip0, the most detailed level, its results are nearly
    // identical to the perfect specular. No reason to just return the default map!
    if ( mip == 0 ) {
        float2 sample_uv = oct_encode( normal );
        uint2 sample_px
            = uint2( sample_uv * float2( HARDCODED_OCTAHEDRAL_SIZE, HARDCODED_OCTAHEDRAL_SIZE ) );

        octahedral_sky_mip[pixel] = octahedral_sky[sample_px];
        return;
    }

    // Sample count should vary per mip-level.
    uint sample_count = 256;
    float total_weight = 0.0f;
    float roughness = octahedral_data.packedfloats0.y;
    float3 prefiltered_color = 0.0f;

    for ( uint i = 0; i < sample_count; i++ ) {
        float2 xi = Hammersley( i, sample_count );
        float3 half = ImportanceSampleGGX( xi, normal, roughness );
        float3 light = normalize( 2.0f * dot( view, half ) * half - view );
        float n_dot_l = max( dot( normal, light ), 0.0f );
        if ( n_dot_l > 0.0f ) {
            float2 sample_uv = oct_encode( light );
            uint2 sample_px = uint2(
                sample_uv * float2( HARDCODED_OCTAHEDRAL_SIZE, HARDCODED_OCTAHEDRAL_SIZE ) );
            prefiltered_color += octahedral_sky[sample_px].rgb * n_dot_l;
            total_weight += n_dot_l;
        }
    }

    prefiltered_color /= total_weight;
    octahedral_sky_mip[pixel] = float4( prefiltered_color, 1.0f );
}


