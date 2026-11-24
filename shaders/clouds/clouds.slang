import cloud_bindings;
import cloud_raymarch;

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
}

FragmentOutput march_pixel( float3 world_pos )
{
    float3 camera_pos = clouds_buffer_data.camera_position.rgb;

    float3 view = normalize( world_pos - camera_pos );

    float4 output_color = raymarch( cumulus_map_LUT, low_freq_noise_LUT,
        linear_repeat_sampler, view, world_pos, clouds_buffer_data.sun_direction );

    FragmentOutput output;

    output.cloud_buffer = output_color;

    return output;
}

// default full resolution raymarch
[shader( "fragment" )]
FragmentOutput clouds_fullres( VertexOutput in )
{
    return march_pixel( in.position );
}

static const int ROTATING_BLOCK_SIZE = 4;
static const int2 ROTATING_PIXEL_ORDER[16] = {
    int2( 0, 0 ), int2( 2, 2 ), int2( 2, 0 ), int2( 0, 2 ),
    int2( 1, 1 ), int2( 3, 3 ), int2( 3, 1 ), int2( 1, 3 ),
    int2( 1, 0 ), int2( 3, 2 ), int2( 3, 0 ), int2( 1, 2 ),
    int2( 0, 1 ), int2( 2, 3 ), int2( 2, 1 ), int2( 0, 3 ),
};

// quarter res raymarch, rotating the target pixel
[shader( "fragment" )]
FragmentOutput clouds_rotating_quarterres( VertexOutput in )
{
    int2 block_offset = ROTATING_PIXEL_ORDER[clouds_buffer_data.rotating_pixel_index & 15];
    float2 subpixel = ( float2( block_offset ) + 0.5f ) / float( ROTATING_BLOCK_SIZE ) - 0.5f;

    float3 world_pos = in.position + ddx( in.position ) * subpixel.x + ddy( in.position ) * subpixel.y;

    return march_pixel( world_pos );
}


