module cloud_raymarch;

import cloud_bindings;
import cloud_utils;
import constants;
import helpers;

#include "../utils.slang"
#include "../common.slang"
#include "../atmosphere/geo/atms_geo.slang"

public float sample_density( Texture2D<float> cumulus_map_LUT, Texture3D<float> low_freq_noise_LUT,
    SamplerState linear_repeat_sampler, float3 march_pos )
{
    float3 p = march_pos * 0.00005f
        + float3( clouds_buffer_data.cloud_offset_x, 0, clouds_buffer_data.cloud_offset_y );

    float cumulus_map = cumulus_map_LUT.Sample( linear_repeat_sampler, p.xz );
    float3 uvw = fract( p );

    float normY = -p.y * 0.5 + 0.5;
    float height_modulation = ( 0.5 * ( -normY + 1 ) ) / ( cumulus_map - 0.2 );
    float value2 = ( 0.5 * ( 10 * normY - 8 ) ) / ( cumulus_map - 0.2 );
    height_modulation
        = 0.5 + smoothstep( 0.6, 0.4, height_modulation ) * smoothstep( 1.0, 0.0, value2 );
    height_modulation
        = ( smoothstep( 3000, 3100, march_pos.y ) + smoothstep( 3500, 3100, march_pos.y ) ) - 1;

    float low_freq_noise = low_freq_noise_LUT.Sample( linear_repeat_sampler, p * 4 );

    float low_freq_modulation = 1.f - height_modulation;

    float low_freq_detail = low_freq_noise_LUT.Sample( linear_repeat_sampler, p * 12 );

    float extra_freq = 4 * low_freq_noise + low_freq_detail;

    cumulus_map = lerp( cumulus_map, cumulus_map + extra_freq, low_freq_modulation );
    cumulus_map = smoothstep( 0.3, 0.6, cumulus_map ) * 2 - 1;

    float base_density = max( cumulus_map * height_modulation, 0 );
    if ( base_density <= 0.f ) {
        return 0.f;
    }

    float high_freq_noise = high_freq_noise_LUT.Sample( linear_repeat_sampler, p * 24 );
    float erosion = clouds_buffer_data.cloud_params.y * high_freq_noise;
    float eroded_density = saturate( ( base_density - erosion ) / ( 1.f - erosion ) );

    return 0.5f * eroded_density;
}


static const float LIGHT_MARCH_MAX_DISTANCE = 5000.0f;
static const int LIGHT_MARCH_STEPS = 6;
static const float LIGHT_MARCH_STEP_SIZES[LIGHT_MARCH_STEPS] = { 30.f, 60.f, 120.f, 240.f, 480.f,
960.f };

static const int VIEW_MARCH_STEPS = 128;
static const int VIEW_MARCH_MAX_ITERATIONS = 256;
static const float VIEW_MARCH_COARSE_SCALE = 3.f;
static const int VIEW_MARCH_EMPTY_SAMPLES_LIMIT = 6;
static const float MAX_CLOUD_DEPTH = 65000.f;

public float estimate_transmittance( Texture2D<float> cumulus_map_LUT,
    Texture3D<float> low_freq_noise_LUT, SamplerState linear_repeat_sampler,
    float3 march_pos, float3 sun_direction )
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

    float T = 1.0;
    float t = 0.f;
    for ( int i = 0; i < LIGHT_MARCH_STEPS; ++i ) {
        float step_size = min( LIGHT_MARCH_STEP_SIZES[i], t_max - t );
        if ( step_size <= 0.f ) {
            break;
        }
        float3 pos = march_pos + local_dir * ( t + 0.5f * step_size ); // midpoint sample
        float d = sample_density(
            cumulus_map_LUT, low_freq_noise_LUT, linear_repeat_sampler, pos );
        // Beer–Lambert per-step
        T *= exp( -clouds_buffer_data.cloud_params.x * d * step_size );
        t += step_size;

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
static const float SIGMA_T = 200.0; // extinction coefficient (tune for opacity)
static const float HG_G = 0.75; // Henyey-Greenstein asymmetry (-1..1), + forward scattering

public float hash11( float n ) { return frac( sin( n ) * 43758.5453 ); }

public float4 raymarch( Texture2D<float> cumulus_map_LUT, Texture3D<float> low_freq_noise_LUT,
    SamplerState linear_repeat_sampler, float3 view, float3 origin, float3 sun_direction,
    out float depth )
{
    float3 local_dir = cloud_dome_remap( view );
    float3 local_origin = origin;

    depth = 0.f;

    if ( view.y < 0.0 ) {
        return float4( 0.f );
    }

    float lower_plane_altitude = 3000;
    float higher_plane_altitude = 3500;

    float t_min = ( lower_plane_altitude - origin.y ) / local_dir.y;
    float t_max = ( higher_plane_altitude - origin.y ) / local_dir.y;

    depth = min( 0.5f * ( t_min + t_max ), MAX_CLOUD_DEPTH );
    float depth_sum = 0.f;
    float depth_weight_sum = 0.f;

    float step_size = ( t_max - t_min ) / (float)VIEW_MARCH_STEPS;
    float coarse_step_size = VIEW_MARCH_COARSE_SCALE * step_size;
    float3 accumulated = float3( 0.0f );

    float throughput = 1.f;

    float sun_horizon_fade = smoothstep( -0.03f, 0.02f, normalize( sun_direction ).y );

    float cloud_radius_km
        = EARTH.bottom_radius + 0.5f * ( lower_plane_altitude + higher_plane_altitude ) * 0.001f;
    float3 sun_color = sample_transmittance_lut( atmosphere_transmittance_LUT, linear_sampler,
                           cloud_radius_km, normalize( sun_direction ).y )
        * EARTH.solar_irradiance * clouds_buffer_data.cloud_params.z / PAPER_WHITE;

    float3 sky_irradiance = get_sun_and_sky_irradiance( atmosphere_irradiance_LUT,
        atmosphere_transmittance_LUT, linear_sampler, float3( 0.f, cloud_radius_km, 0.f ),
        float3( 0.f, 1.f, 0.f ), normalize( sun_direction ) )
                                ._1;
    float3 ambient_radiance
        = sky_irradiance * INV_PI * clouds_buffer_data.cloud_params.z / PAPER_WHITE;

    // jitter
    float jitter = 0.3f
        * ( rand_float( dot( local_origin.xyz, float3( 12.9898, 78.233, 45.164 ) ) ) - 0.5 )
        * step_size;
    float t_start = t_min + 0.5f * step_size + jitter;
    float t = t_start;
    bool in_cloud = false;
    int empty_samples = 0;
    for ( int i = 0; i < VIEW_MARCH_MAX_ITERATIONS; ++i ) {
        if ( throughput < 0.01 ) {
            break;
        }
        if ( t > t_max ) {
            break;
        }
        float3 marched_pos = local_origin + local_dir * t;
        float density = sample_density(
            cumulus_map_LUT, low_freq_noise_LUT, linear_repeat_sampler, marched_pos );

        if ( !in_cloud ) {
            if ( density > 0.f ) {
                in_cloud = true;
                empty_samples = 0;
                t = max( t - coarse_step_size + step_size, t_start );
            }
            else {
                t += coarse_step_size;
            }
            continue;
        }

        if ( density > 0.01 ) {
            empty_samples = 0;

            float transmittance
                = max( estimate_transmittance( cumulus_map_LUT, low_freq_noise_LUT,
                           linear_repeat_sampler, marched_pos, sun_direction ),
                    0.01 );

            float cosTheta = dot( local_dir, sun_direction );
            float phase
                = 0.95 * PhaseHG( cosTheta, 0.75 ) + 0.05 * PhaseHG( cosTheta, -0.2 ); // TTHG

            float step_transmittance
                = exp( -density * step_size * clouds_buffer_data.cloud_params.x );

            float3 direct_lighting = sun_color * ALBEDO * phase * transmittance
                * sun_horizon_fade * ( 1.f - step_transmittance );
            float3 ambient_lighting = ALBEDO * ambient_radiance * ( 1.f - step_transmittance );
            accumulated += throughput * ( direct_lighting + ambient_lighting );

            float depth_weight = throughput * ( 1.f - step_transmittance );
            depth_sum += t * depth_weight;
            depth_weight_sum += depth_weight;

            throughput *= step_transmittance;
        }
        else {
            empty_samples++;
            if ( empty_samples >= VIEW_MARCH_EMPTY_SAMPLES_LIMIT ) {
                in_cloud = false;
            }
        }

        t += step_size;
    }

    if ( depth_weight_sum > 0.f ) {
        depth = min( depth_sum / depth_weight_sum, MAX_CLOUD_DEPTH );
    }

    if ( length( accumulated ) < 1e-6 ) {
        return float4( 0.f );
    }

    float3 color = accumulated * PAPER_WHITE;

    float opacity = 1.f - throughput;

    if ( view.y < 0.1 ) {
        opacity *= view.y * 10.f;
        color *= view.y * 10.f;
    }

    return float4( color, saturate( opacity ) );
}


