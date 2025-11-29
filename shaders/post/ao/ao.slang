#include "../../common.slang"
#include "../../utils.slang"

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_data;

layout( binding = 0, set = 1 ) RWTexture2D<float4> output;
layout( binding = 1, set = 1 ) Texture2D<float4> scene_color;
layout( binding = 2, set = 1 ) Texture2D<float4> gbuffer_normal;
layout( binding = 3, set = 1 ) Texture2D<float4> gbuffer_depth;

const static uint SLICES = 5;
const static float RADIUS = 1;

const static float FOV = 60.0f;

uint2 view_to_pixel( float3 view_pos )
{
    uint width, height;
    gbuffer_depth.GetDimensions( width, height );

    float fx = 1.0f / tan( 0.5f * FOV );
    float fy = fx * height / width;

    // This is just the inverse of pixel_to_view.
    // In theory, the math should keep it consistent, which is good
    float2 ndc;
    ndc.x = saturate( 0.5f * ( view_pos.x * fx / view_pos.z ) + 0.5f );
    ndc.y = saturate( 0.5f * ( view_pos.y * fy / view_pos.z ) + 0.5f );

    return uint2( uint( ndc.x * width ), uint( ndc.y * height ) );
}

float3 pixel_to_view( uint2 pixel )
{
    uint width, height;
    gbuffer_depth.GetDimensions( width, height );

    float linear_depth = sample_linear_depth( pixel );

    float2 ndc;
    ndc.x = 2.0f * ( float( pixel.x ) + 0.5f ) / float( width ) - 1.0f;
    ndc.y = 2.0f * ( float( pixel.y ) + 0.5f ) / float( height ) - 1.0f;

    float fx = 1.0f / tan( 0.5f * FOV );
    float fy = fx * height / width;

    float3 view_pos = float3( ndc.x / fx, ndc.y / fy, 1.0f );
    return view_pos * linear_depth;
}

float3 world_nor_to_view( float3 normal ) { return mul( (float3x3)camera_data.view_mat, normal ); }

float sample_linear_depth( uint2 pixel_sample )
{
    float depth = gbuffer_depth[pixel_sample].r;

    float ndc_depth = 2.0f * depth - 1.0f;

    // Will the compiler cache this constant result?
    return LinearDepth( ndc_depth, camera_data.camera_constants.x, camera_data.camera_constants.y );
}

float SSAO( float3 view_position, float3 view_normal )
{
    float g = 1.220744084605759;

    float occlusion = 0.0f;
    float samples = 0.0f;

    for ( uint i = 0; i < SLICES; i++ ) {
        float2 rng = frac( i * g );
        // Assume one step per direction for now
        float3 view_offset = get_cos_vec( view_normal, rng );

        // Convert to screenspace back
        uint2 pixel_sample = view_to_pixel( 0.05f * view_offset + view_position );
        float sample_depth = sample_linear_depth( pixel_sample );

        occlusion += select( sample_depth >= view_position.z + 0.01f, 1.0f, 0.0f );
        samples += 1.0f;
    }

    return 1.0f - ( occlusion / samples );
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

    if ( dot( normal, sphere ) < 0.0f ) {
        sphere = -sphere;
    }

    return sphere;
}

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
func cs_ao( uint2 thread_id: SV_DispatchThreadID )->void
{
    uint2 pixel = thread_id.xy;

    // World-space normal
    float3 normal = gbuffer_normal[pixel].rgb;

    // Acquire view_position? somehow...
    // The "view_pos" I want is something that the view space normal can offset from.
    // The only goal for it is to essentially be an origin for our next sample, which we will
    // sample the same depth from for consistency.
    float3 view_pos = pixel_to_view( pixel );
    float3 view_normal = world_nor_to_view( normal );

    float ao = SSAO( view_pos, view_normal );

    float3 result = float3( 0.0f, ao, 0.0f ); // sample_linear_depth( pixel );

    output[pixel] = float4( result, 1.f );
}


