import constants;
import "../clouds/cloud_bindings.slang";
import "../clouds/cheap_raymarch.slang";

// desc set 0, 1, 2 in cloud_bindings.slang

layout( binding = 0, set = 3 ) RWStructuredBuffer<float> sun_visibility;

struct GameStateBuffer {
    float4 world_position;
};

layout( binding = 0, set = 4 ) ConstantBuffer<GameStateBuffer> gamestate_data;

static const uint SUN_SAMPLES_PER_SIDE = 8;
static const uint SUN_SAMPLE_COUNT = SUN_SAMPLES_PER_SIDE * SUN_SAMPLES_PER_SIDE;

static const uint MAX_WAVES = SUN_SAMPLE_COUNT;

static const float CLOUD_LAYER_BOTTOM = 3000.0f;
static const float CLOUD_LAYER_TOP = 3500.0f;
static const uint TRANSMITTANCE_STEPS = 32;

static const float SUN_EXTINCTION = 0.03f;

groupshared float wave_visibility[MAX_WAVES];

float3 sample_sun_disk( uint sample_index, float3 sun_direction )
{
    uint2 grid = uint2( sample_index % SUN_SAMPLES_PER_SIDE, sample_index / SUN_SAMPLES_PER_SIDE );
    float2 xi = ( float2( grid ) + 0.5f ) / float( SUN_SAMPLES_PER_SIDE );

    float radius = sqrt( xi.x );
    float phi = 2.0f * PI * xi.y;
    float2 disk = radius * float2( cos( phi ), sin( phi ) );

    float3 up = select(
        abs( sun_direction.y ) < 0.999f, float3( 0.0f, 1.0f, 0.0f ), float3( 1.0f, 0.0f, 0.0f ) );
    float3 tangent = normalize( cross( up, sun_direction ) );
    float3 bitangent = cross( sun_direction, tangent );

    float extent = tan( SUN_ANGULAR_RADIUS );

    return normalize( sun_direction + extent * ( disk.x * tangent + disk.y * bitangent ) );
}

float sun_transmittance( float3 direction )
{
    // below the horizon  
    if ( direction.y <= 0.0f ) {
        return 0.0f;
    }

    float3 origin = gamestate_data.world_position.xyz;

    float3 texture_offset
        = float3( clouds_buffer_data.cloud_offset_x, 0.0f, clouds_buffer_data.cloud_offset_y );

    float t_min = ( CLOUD_LAYER_BOTTOM - origin.y ) / direction.y;
    float t_max = ( CLOUD_LAYER_TOP - origin.y ) / direction.y;

    float step_size = ( t_max - t_min ) / float( TRANSMITTANCE_STEPS );

    float transmittance = 1.0f;

    for ( uint i = 0; i < TRANSMITTANCE_STEPS; i++ ) {
        float t = t_min + ( float( i ) + 0.5f ) * step_size;

        float density = sample_density( cumulus_map_LUT, low_freq_noise_LUT,
            linear_mirrored_repeat_sampler, origin + direction * t, texture_offset );

        transmittance *= exp( -SUN_EXTINCTION * density * step_size );

        if ( transmittance <= 1e-4f ) {
            return 0.0f;
        }
    }

    return saturate( transmittance );
}

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void cs_sun_visibility( uint thread_id: SV_GroupIndex )
{
    float3 sun_direction = normalize( clouds_buffer_data.sun_direction );
    float3 sample_direction = sample_sun_disk( thread_id, sun_direction );

    float visibility = sun_transmittance( sample_direction );

    uint lane_count = WaveGetLaneCount();
    float wave_sum = WaveActiveSum( visibility );

    if ( WaveIsFirstLane() ) {
        wave_visibility[thread_id / lane_count] = wave_sum;
    }

    GroupMemoryBarrierWithGroupSync();

    if ( thread_id == 0 ) {
        uint wave_count = ( SUN_SAMPLE_COUNT + lane_count - 1 ) / lane_count;

        float total = 0.0f;
        for ( uint wave = 0; wave < wave_count; wave++ ) {
            total += wave_visibility[wave];
        }

        sun_visibility[0] = total / float( SUN_SAMPLE_COUNT );
    }
}
