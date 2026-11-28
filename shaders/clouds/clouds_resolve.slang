#include "../common.slang"
#include "cloud_uniforms.slang"
import cloud_utils;

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_buffer_data;
layout( binding = 1, set = 0 ) Texture2D<float4> rotating_cloud_buffer;
layout( binding = 2, set = 0 ) ConstantBuffer<CloudUniformData> clouds_buffer_data;
layout( binding = 3, set = 0 ) SamplerState linear_sampler;
layout( binding = 4, set = 0 ) Texture2D<float4> cloud_history;
layout( binding = 5, set = 0 ) Texture2D<float> cloud_depth_history;
layout( binding = 6, set = 0 ) Texture2D<float> rotating_cloud_depth;

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT0;
    float2 uv : TEXCOORD0;
};

struct VertexOutput {
    float4 sv_position : SV_POSITION;
    float3 position;
};

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    VertexOutput output;

    let clip_position = float4( input.position, 1.f );
    let view_position = mul( clouds_buffer_data.inverse_proj, clip_position );
    let world_position = mul( clouds_buffer_data.inverse_view, float4( view_position.xyz, 1.f ) );

    output.sv_position = clip_position;
    output.position = world_position.xyz;

    return output;
}

struct FragmentOutput {
    float4 cloud_buffer : SV_Target0;
    float cloud_depth : SV_Target1;
}

[shader( "fragment" )]
FragmentOutput fs_main( VertexOutput input )
{
    FragmentOutput output;

    int2 pixel = int2( input.sv_position.xy );
    int2 qres_fragcoord = pixel >> 2;
    int2 frag_offset = pixel & 0b11;
    int2 block_offset = ROTATING_PIXEL_ORDER[clouds_buffer_data.rotating_pixel_index & 15];

    // there's a circular dependency for depth, so unfortunately
    // we have to interpolate depth from qres and use it for our
    // reprojection
    float cloud_depth_qres = rotating_cloud_depth.Load( int3( int2( qres_fragcoord ), 0 ) );

    // we are the target texel for this frame
    if ( all( block_offset == frag_offset ) ) 
    {
        output.cloud_buffer = rotating_cloud_buffer.Load( int3( int2( qres_fragcoord ), 0 ) );
        output.cloud_depth = cloud_depth_qres;
        return output;
    }

    float3 view = normalize( input.position - clouds_buffer_data.camera_position );

    if ( view.y < 0.f ) {
        output.cloud_buffer = float4( 0.f );
        output.cloud_depth = 0.f;
        return output;
    }

    float3 local_dir = cloud_dome_remap( view );
    float depth = cloud_depth_qres > 0.f
        ? cloud_depth_qres
        : ( CLOUD_MID_ALTITUDE - input.position.y ) / local_dir.y;

    float3 cloud_pos = input.position + local_dir * depth;
    cloud_pos += float3( clouds_buffer_data.cloud_offset_x - clouds_buffer_data.prev_cloud_offset_x,
                     0.f, clouds_buffer_data.cloud_offset_y - clouds_buffer_data.prev_cloud_offset_y )
        / CLOUD_NOISE_SCALE;

    float3 prev_view = cloud_dome_unmap( normalize( cloud_pos - input.position ) );
    float4 prev_clip = mul( camera_buffer_data.prev_mvp, float4( prev_view, 0.f ) );
    float2 prev_uv = prev_clip.xy / prev_clip.w * 0.5f + 0.5f;

    // reproject
    if ( prev_clip.w > 0.f && all( prev_uv >= 0.f ) && all( prev_uv <= 1.f ) ) {
        output.cloud_buffer = cloud_history.SampleLevel( linear_sampler, prev_uv, 0 );
        output.cloud_depth = cloud_depth_history.SampleLevel( linear_sampler, prev_uv, 0 );
        return output;
    }

    // fallback interpolation
    uint qres_width, qres_height;
    rotating_cloud_buffer.GetDimensions( qres_width, qres_height );

    float2 qres_uv = ( float2( pixel - block_offset ) / float( ROTATING_BLOCK_SIZE ) + 0.5f )
        / float2( qres_width, qres_height );

    output.cloud_buffer = rotating_cloud_buffer.SampleLevel( linear_sampler, qres_uv, 0 );
    output.cloud_depth = rotating_cloud_depth.SampleLevel( linear_sampler, qres_uv, 0 );

    return output;
}