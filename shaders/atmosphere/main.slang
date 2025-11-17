import atmosphere;
import bindings;

struct VertexInput {
    float2 position : POSITION;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    float3 view_ray;
};

static const float3 GAMMA_FACTOR = float3( 1.f / 2.2f );

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    let clip_position = float4( input.position, 0.f, 1.f );
    let view_position = mul( clip_position, uniforms.inverse_proj );
    let world_position = mul( view_position, uniforms.inverse_view );

    var output : VertexOutput;
    output.sv_position = clip_position;
    output.view_ray = world_position.xyz;

    return output;
}

[shader( "fragment" )]
float4 fs_main( VertexOutput input )
    : SV_Target
{
    let view_ray = input.view_ray;
    let view_dir = normalize( view_ray );
    let camera_pos = uniforms.camera_position;
    let earth_center = uniforms.earth_center.xyz;
    let sun_dir = normalize( uniforms.sun_direction );
    let sun_size = float2( uniforms.sun_size_x, uniforms.sun_size_y );
    let exposure = uniforms.exposure;
    let white_point = uniforms.white_point;

    let sky
        = get_sky_radiance( ATMOSPHERE_PARAMETERS, camera_pos - earth_center, view_dir, sun_dir );

    var radiance = sky.radiance;
    if ( dot( view_dir, sun_dir ) > sun_size.y ) {
        radiance = float3( 1.f, 0.f, 0.f );
    }

    // Apply tone mapping via Reinhard and sRGB gamma correction
    let color = pow( float3( 1.f ) - exp( -radiance / white_point * exposure ), GAMMA_FACTOR );

    return float4( color, 1.f );
}


