#include "../../common.slang"
#include "../../utils.slang"

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_data;
layout( binding = 1, set = 0 ) ConstantBuffer<AOData> ao_data;

layout( binding = 0, set = 1 ) RWTexture2D<float4> output;
layout( binding = 1, set = 1 ) Texture2D<float4> scene_color;
layout( binding = 2, set = 1 ) Texture2D<float4> gbuffer_normal;
layout( binding = 3, set = 1 ) Texture2D<float4> gbuffer_depth;

const static uint SLICES = 20;
const static float RADIUS = 1;

const static float FOV = 60.0f;

const static float SCREEN_WIDTH = 1920.0f;
const static float SCREEN_HEIGHT = 1080.0f;

float3 world_nor_to_view( float3 normal ) { return mul( (float3x3)camera_data.view_mat, normal ); }

float sample_linear_depth( uint2 pixel_sample )
{
    float depth = gbuffer_depth[pixel_sample].r;

    float ndc_depth = 2.0f * depth - 1.0f;

    // Will the compiler cache this constant result?
    return LinearDepth( ndc_depth, camera_data.camera_constants.x, camera_data.camera_constants.y );
}

float2 rng_from_float( float x )
{
    // Convert float to uint-ish domain
    uint xi = asuint( x * 1e4f ); // scale to get some bits
    xi = ( xi ^ 61u ) ^ ( xi >> 16 );
    xi *= 9u;
    xi = xi ^ ( xi >> 4 );
    xi *= 0x27d4eb2du;
    xi = xi ^ ( xi >> 15 );

    float x_rand = frac( float( xi ) * 0.000000059604644775390625f ); // 1/2^24
    float y_rand = frac( float( xi * 0x7f4a7c15u ) * 0.000000059604644775390625f );

    return float2( x_rand, y_rand );
}

uint2 view_to_pixel( float3 view_pos )
{
    float4 clip_pos = mul( camera_data.proj_mat, float4( view_pos, 1.0f ) );
    clip_pos.xyz /= clip_pos.w;

    float2 ndc_coord = clip_pos.xy;
    float2 normalized_ss = 0.5f * ndc_coord + 0.5f;
    float2 ss_pos = normalized_ss * float2( SCREEN_WIDTH, SCREEN_HEIGHT );

    return uint2( ss_pos );
}

float3 SSAO( float3 view_position, float3 view_normal )
{
    float g = 1.220744084605759;

    float occlusion = 0.0f;
    float samples = 0.0f;

    float test_depth;
    float sample_linear_depth;
    float truth_depth;

    for ( uint i = 0; i < 8; i++ ) {
        float2 rng = rng_from_float( g + view_position.x + view_position.y + float( i ) );
        // Assume one step per direction for now
        float radius = 1.0f * ao_data.packed_floats0.y;
        float3 view_offset = get_cos_vec( view_normal, rng );
        view_offset = radius * normalize( view_offset + 0.2f * view_normal );

        float3 sample_view_pos = view_offset + view_position;
        float4 sample_clip_pos = mul( camera_data.proj_mat, float4( sample_view_pos, 1.0f ) );
        float sample_ndc_z = sample_clip_pos.z / sample_clip_pos.w;
        sample_linear_depth = LinearDepth( sample_ndc_z * 0.5f + 0.5f, 0.1f, 100.0f );

        // in clip-z space... flip sign to be positive
        // x: [-inf, inf]
        // y: [-inf, inf]
        // z: [-near, -far] currently, I think

        // Convert to screenspace back
        uint2 pixel_sample = view_to_pixel( sample_view_pos );

        if ( pixel_sample.x >= SCREEN_WIDTH || pixel_sample.y >= SCREEN_HEIGHT ) {
            continue;
        }

        // Clip-space depth
        truth_depth = gbuffer_depth[pixel_sample].r;
        // LINEAR DEPTH TAKES IN A VALUE FROM 0-1!!!!
        truth_depth = LinearDepth( truth_depth, 0.1f, 100.0f );

        float3 sample_offset = sample_view_pos - view_position;
        float dist = length( sample_offset );
        float falloff = saturate( 1.0 - dist / radius );
        float n_dot_d = saturate( dot( view_normal, normalize( sample_offset ) ) );

        // Depth check
        bool rangeCheck = abs( sample_linear_depth - truth_depth ) < radius + 0.1f;

        float offset = ao_data.packed_floats0.z;
        bool isOccluded = sample_linear_depth - offset > truth_depth;
        occlusion += select( isOccluded && rangeCheck, 1.0f, 0.0f );
        samples += 1.0f;
    }

    float thickness = 1.5f * ao_data.packed_floats0.y;
    float ao = saturate( 1.0f - ( occlusion / samples ) * ( 1.0f + thickness ) );

    return float3( ao );
}

// Very clever sampling trick. Thanks, Zenteon.
// https://github.com/Zenteon/ZenteonFX/blob/main/Shaders/Zenteon_SSAO_History.fx
float3 get_cos_vec( float3 normal, float2 rng )
{
    rng.y = rng.y * 2.0f - 1.0f;

    float3 sphere;
    sincos( 6.28f * rng.x, sphere.y, sphere.x );
    sphere.xy *= sqrt( 1.0f - rng.y * rng.y );
    sphere.z = rng.y;

    sphere = normalize( sphere );

    if ( dot( normal, sphere ) < 0.0f ) {
        sphere = -sphere;
    }

    return sphere;
}

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
func cs_ao( uint2 thread_id: SV_DispatchThreadID )->void
{
    bool enable_ao = ao_data.packed_floats1.x > 0.5f;
    uint2 pixel = thread_id.xy;

    if ( !enable_ao ) {
        output[pixel] = scene_color[pixel];
        return;
    }

    // World-space normal
    float3 normal = gbuffer_normal[pixel].rgb;

    // Hardcoded screen dimensions
    float2 uv = ( float2( pixel ) + 0.5f ) / float2( SCREEN_WIDTH, SCREEN_HEIGHT );

    // Reconstruct world-pos
    float depth = gbuffer_depth[pixel].r;
    float ndc_depth = depth * 2.0f - 1.0f;

    float4 clip_pos = float4( uv * 2.0f - 1.0f, ndc_depth, 1.0f );
    float4 view_pos_h = mul( camera_data.inv_proj, clip_pos );

    // this is NOT NDC depth
    float3 view_pos = ( view_pos_h.xyz / view_pos_h.w );
    float3 view_normal = normalize( mul( (float3x3)camera_data.view_mat, normal ) );

    float3 ao = SSAO( view_pos, view_normal );

    bool enable_debug = ao_data.packed_floats0.w > 0.5f;
    float3 result = select( enable_debug, ao, ao * scene_color[pixel].rgb );

    output[pixel] = float4( result, 1.f );
}


