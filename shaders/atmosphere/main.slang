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
static const float3 GROUND_ALBEDO = float3( 0.f, 0.f, 0.04f );

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

    var ground_alpha = 0.f;
    var ground_radiance = float3( 0.f );
    let earth_offset = camera_pos - earth_center;
    let earth_dot_v = dot( earth_offset, view_dir );
    let earth_dot_earth = dot( earth_offset, earth_offset );
    let earth_radius_sq = EARTH.bottom_radius * EARTH.bottom_radius;
    let ray_center_sq = earth_dot_earth - earth_dot_v * earth_dot_v;
    let ground_discriminant = earth_radius_sq - ray_center_sq;
    if ( ground_discriminant >= 0.f ) {
        let ground_distance = -earth_dot_v - sqrt( ground_discriminant );

        if ( ground_distance > 0.f ) {
            let point = camera_pos + view_dir * ground_distance;
            let normal = normalize( point - earth_center );
            let irradiance
                = get_sun_and_sky_irradiance( EARTH, point - earth_center, normal, sun_dir );

            ground_radiance = INV_PI * irradiance.sun * irradiance.sky;

            let scatter = get_sky_radiance_to_point(
                EARTH, camera_pos - earth_center, point - earth_center, sun_dir );

            ground_radiance *= scatter.transmittance + scatter.radiance;
            ground_alpha = 1.f;
        }
    }

    let sky = get_sky_radiance( EARTH, camera_pos - earth_center, view_dir, sun_dir );

    var radiance = sky.radiance;
    if ( dot( view_dir, sun_dir ) > sun_size.y ) {
        let solar_radiance
            = EARTH.solar_irradiance / ( PI * EARTH.sun_angular_radius * EARTH.sun_angular_radius );
        radiance += sky.transmittance * solar_radiance;
    }

    // radiance = lerp( radiance, ground_radiance, ground_alpha );

    // Apply tone mapping via Reinhard and sRGB gamma correction
    let color = pow( float3( 1.f ) - exp( -radiance / white_point * exposure ), GAMMA_FACTOR );

    return float4( color, 1.f );
}


