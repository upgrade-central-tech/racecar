module cheap_raymarch;

// import cloud_bindings;
import cloud_utils;

#include "../utils.slang"

public float sample_density( Texture2D<float> cumulus_map_LUT, Texture3D<float> low_freq_noise_LUT,
    Texture3D<float> high_freq_noise_LUT, SamplerState linear_mirrored_repeat_sampler,
    float3 march_pos, float3 texture_offset, float erosion )
{
    float3 p = march_pos * 0.00005f + texture_offset;

    float cumulus_map = cumulus_map_LUT.Sample( linear_mirrored_repeat_sampler, p.xz );
    float3 uvw = fract( p );

    float normY = -p.y * 0.5 + 0.5;
    float height_modulation = ( 0.5 * ( -normY + 1 ) ) / ( cumulus_map - 0.2 );
    float value2 = ( 0.5 * ( 10 * normY - 8 ) ) / ( cumulus_map - 0.2 );
    height_modulation
        = 0.5 + smoothstep( 0.6, 0.4, height_modulation ) * smoothstep( 1.0, 0.0, value2 );
    height_modulation
        = ( smoothstep( 3000, 3100, march_pos.y ) + smoothstep( 3500, 3100, march_pos.y ) ) - 1;

    float low_freq_noise = low_freq_noise_LUT.Sample( linear_mirrored_repeat_sampler, p * 4 );
    float low_freq_modulation = 1.f - height_modulation;

    float low_freq_detail = low_freq_noise_LUT.Sample( linear_mirrored_repeat_sampler, p * 12 );

    float extra_freq = 4 * low_freq_noise + low_freq_detail;

    cumulus_map = lerp( cumulus_map, cumulus_map + extra_freq, low_freq_modulation );
    cumulus_map = smoothstep( 0.3, 0.6, cumulus_map ) * 2 - 1;

    float base_density = max( cumulus_map * height_modulation, 0 );
    if ( base_density <= 0.f ) {
        return 0.f;
    }

    float high_freq_noise = high_freq_noise_LUT.Sample( linear_mirrored_repeat_sampler, p * 24 );
    float erosion_amount = erosion * high_freq_noise;
    float eroded_density
        = saturate( ( base_density - erosion_amount ) / ( 1.f - erosion_amount ) );

    return 0.5f * eroded_density;
}

static const float LIGHT_MARCH_MAX_DISTANCE = 5000.0f;

public float estimate_transmittance( Texture2D<float> cumulus_map_LUT,
    Texture3D<float> low_freq_noise_LUT, Texture3D<float> high_freq_noise_LUT,
    SamplerState linear_mirrored_repeat_sampler, float3 march_pos, float3 sun_direction,
    float3 texture_offset, float extinction, float erosion )
{
    float3 local_dir = normalize( sun_direction );

    float lower_plane_altitude = 3000;
    float higher_plane_altitude = 3500;
    if ( march_pos.y > higher_plane_altitude || march_pos.y < lower_plane_altitude ) {
        return 1.f;
    }

    float t_max = LIGHT_MARCH_MAX_DISTANCE;
    if ( local_dir.y > 1e-4 ) {
        t_max = min( t_max, ( higher_plane_altitude - march_pos.y ) / local_dir.y );
    } else if ( local_dir.y < -1e-4 ) {
        t_max = min( t_max, ( lower_plane_altitude - march_pos.y ) / local_dir.y );
    }

    uint steps = 16 >> 3;

    float t0 = 1e-3;
    float step_size = t_max / (float)steps;

    float T = 1.0;
    for ( int i = 0; i < steps; ++i ) {
        float t = t0 + ( i + 0.5 ) * step_size; // midpoint sample
        float3 pos = march_pos + local_dir * t;
        // if outside the AABB you can break early, but caller will pass proper t_max
        float d = sample_density( cumulus_map_LUT, low_freq_noise_LUT, high_freq_noise_LUT,
            linear_mirrored_repeat_sampler, pos, texture_offset, erosion );
        // Beer–Lambert per-step
        T *= exp( -extinction * d * step_size );

        if ( T <= 1e-4 ) {
            return 0.0;
        } // early out: essentially fully occluded
    }
    return saturate( T );
}

public float PhaseHG( float cosTheta, float g )
{
    float g2 = g * g;
    float denom = pow( 1 + g2 - 2 * g * cosTheta, 1.5 );
    return ( 1 - g2 ) / ( 4 * 3.14159265 * denom );
}

static const float ALBEDO = 0.9; // single-scattering albedo
static const float HG_G = 0.75; // Henyey-Greenstein asymmetry (-1..1), + forward scattering

public float hash11( float n ) { return frac( sin( n ) * 43758.5453 ); }

public float offset_sphere_intersection(
    float3 view, float3 origin, float surface_level, float rad )
{
    // a Triangle forms between the earth's core, the current position, and the intersection point
    float obtuse_angle = PI;
    if ( length( float3( view.x, 0, view.z ) ) > 0.01 ) {
        float3 ground_vector = normalize( float3( view.x, 0, view.z ) );
        float angle_to_ground = acos( dot( view, ground_vector ) );
        obtuse_angle = angle_to_ground + PI / 2;
    }

    // use law of sines to find t
    float atmo_angle = asin( surface_level * sin( obtuse_angle ) / rad );
    float core_angle = PI - atmo_angle - obtuse_angle;
    float t = rad * sin( core_angle ) / sin( obtuse_angle );

    return t;
}

public float4 raymarch( Texture2D<float> cumulus_map_LUT, Texture3D<float> low_freq_noise_LUT,
    Texture3D<float> high_freq_noise_LUT, SamplerState linear_mirrored_repeat_sampler, float3 view,
    float3 origin, float3 sun_direction, float3 sun_color, float3 ambient_radiance,
    float3 texture_offset, float extinction, float erosion )
{
    float theta = acos( saturate( view.y ) );
    // theta = clamp( theta, 0.0, radians( 80 ) );

    float lerp = ( theta ) / PI * 2.f;

    float thetaRemap = lerp * radians( 80 );
    float phi = atan2( view.z, view.x );

    float sinTheta = sin( thetaRemap );
    float3 local_dir = float3( sinTheta * cos( phi ), cos( thetaRemap ), sinTheta * sin( phi ) );
    float3 local_origin = origin;

    if ( view.y < 0.0 ) {
        return float4( 0.f );
    }

    float lower_plane_altitude = 3000;
    float higher_plane_altitude = 3500;

    float t_min = ( lower_plane_altitude - origin.y ) / local_dir.y;
    float t_max = ( higher_plane_altitude - origin.y ) / local_dir.y;

    int steps = 40 >> 2;

    float t = 0.0f;
    float step_size = ( t_max - t_min ) / (float)steps;
    float3 accumulated = float3( 0.0f );

    float throughput = 1.f;

    float sun_horizon_fade = smoothstep( -0.03f, 0.02f, normalize( sun_direction ).y );

    float3 sun_irradiance = sun_color / PAPER_WHITE;
    float3 ambient = ambient_radiance / PAPER_WHITE;
    // jitter
    float jitter = 0.3f
        * ( rand_float( dot( local_origin.xyz, float3( 12.9898, 78.233, 45.164 ) ) ) - 0.5 )
        * step_size;
    for ( int i = 0; i < steps; ++i ) {
        float t = t_min + ( i + 0.5 ) * step_size + jitter;
        if ( throughput < 0.01 ) {
            break;
        }
        if ( t < t_min ) {
            continue;
        }
        if ( t > t_max ) {
            break;
        }
        float3 marched_pos = local_origin + local_dir * t;
        float density = sample_density( cumulus_map_LUT, low_freq_noise_LUT, high_freq_noise_LUT,
            linear_mirrored_repeat_sampler, marched_pos, texture_offset, erosion );
        if ( density > 0.01 ) {

            float transmittance = max(
                estimate_transmittance( cumulus_map_LUT, low_freq_noise_LUT, high_freq_noise_LUT,
                    linear_mirrored_repeat_sampler, marched_pos, sun_direction, texture_offset,
                    extinction, erosion ),
                0.01 );

            float cosTheta = dot( local_dir, sun_direction );
            float phase
                = 0.95 * PhaseHG( cosTheta, 0.75 ) + 0.05 * PhaseHG( cosTheta, -0.2 ); // TTHG

            float step_transmittance = exp( -density * step_size * extinction );

            float3 direct_lighting = sun_irradiance * ALBEDO * phase * transmittance
                * sun_horizon_fade * ( 1.f - step_transmittance );
            float3 ambient_lighting = ALBEDO * ambient * ( 1.f - step_transmittance );
            accumulated += throughput * ( direct_lighting + ambient_lighting );
            throughput *= step_transmittance;
        }
    }
    if ( length( accumulated ) < 1e-6 ) {
        return float4( 0.f );
    }

    float3 color = accumulated * PAPER_WHITE;

    float opacity = 1.f - throughput;

    if ( view.y < 0.1 ) {
        float horizon_fade = view.y * 10.f;
        opacity *= horizon_fade;
        color *= horizon_fade;
    }

    return float4( color, saturate( opacity ) );
}


