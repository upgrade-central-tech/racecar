import constants;
import "../clouds/cloud_bindings.slang";
import "../clouds/cheap_raymarch.slang";
import "../clouds/cloud_utils.slang";

#include "../common.slang"

// desc set 0, 1, 2 in cloud_bindings.slang

layout( binding = 0, set = 3 ) RWTexture2D<float> sun_visibility_shadow_map;

layout( binding = 1, set = 3 ) ConstantBuffer<TerrainSizeData> terrain_size_data;

layout( binding = 0, set = 4 ) ConstantBuffer<GameStateBuffer> gamestate_data;

static const float CLOUD_LAYER_BOTTOM = 3000.0f;
static const float CLOUD_LAYER_TOP = 3500.0f;
static const uint TRANSMITTANCE_STEPS = 32;
static const float SHADOW_EXTINCTION_SCALE = 0.5f;

float sun_transmittance( float3 origin, float3 direction )
{
    if ( direction.y <= 0.0f ) {
        return 0.0f;
    }

    float3 texture_offset = float3(
        clouds_buffer_data.cloud_offset_x,
        0.0f,
        clouds_buffer_data.cloud_offset_y
    );

    float t_min = ( CLOUD_LAYER_BOTTOM - origin.y ) / direction.y;
    float t_max = ( CLOUD_LAYER_TOP - origin.y ) / direction.y;

    float step_size = ( t_max - t_min ) / float( TRANSMITTANCE_STEPS );

    float transmittance = 1.0f;

    for ( uint i = 0; i < TRANSMITTANCE_STEPS; i++ ) {
        float t = t_min + ( float( i ) + 0.5f ) * step_size;

        float density = sample_density(
            cumulus_map_LUT,
            low_freq_noise_LUT,
            high_freq_noise_LUT,
            linear_mirrored_repeat_sampler,
            origin + direction * t,
            texture_offset,
            clouds_buffer_data.cloud_params.y
        );

        transmittance *= exp(
            -clouds_buffer_data.cloud_params.x * SHADOW_EXTINCTION_SCALE * density * step_size
        );

        if ( transmittance <= 1e-4f ) {
            return 0.0f;
        }
    }

    return saturate( transmittance );
}

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void cs_sun_visibility( uint3 thread_id: SV_DispatchThreadID )
{
    uint width, height;
    sun_visibility_shadow_map.GetDimensions( width, height );

    if ( thread_id.x >= width || thread_id.y >= height ) {
        return;
    }

    float2 uv = ( float2( thread_id.xy ) + 0.5f ) / float2( width, height );

    float2 terrain_point_xz = terrain_size_data.center.xz
        + ( ( uv - 0.5f ) * terrain_size_data.size );
    float3 terrain_point = float3(
        terrain_point_xz.x,
        terrain_size_data.center.y,
        terrain_point_xz.y
    );

    float3 sun_dir = normalize( clouds_buffer_data.sun_direction );

    if ( sun_dir.y <= 0.0f ) {
        sun_visibility_shadow_map[thread_id.xy] = 0.0f;
        return;
    }

    sun_visibility_shadow_map[thread_id.xy]
        = sun_transmittance( terrain_point, cloud_dome_remap( sun_dir ) );
}
