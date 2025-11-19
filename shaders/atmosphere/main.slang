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
static const float3 GROUND_ALBEDO = { 0.f, 0.f, 0.04f };
static const float RADIANCE_EXPOSURE = 10.f;

/// Expressed in radians.
static const float SUN_ANGULAR_RADIUS = 0.04f;
static const float SUN_SIZE = cos( SUN_ANGULAR_RADIUS );

/// Earth radius is 6360 km, which we represent as height. This is a negative value because
/// everything is relative to the Earth's surface.
static const float3 EARTH_CENTER = { 0.f, -6360.f, 0.f };

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    let clip_position = float4( input.position, 0.f, 1.f );
    let view_position = mul( uniforms.inverse_proj, clip_position );
    let world_position = mul( uniforms.inverse_view, float4( view_position.xyz, 0.f ) );

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
    let sun_dir = normalize( uniforms.sun_direction );

    var ground_alpha = 0.f;
    var ground_radiance = float3( 0.f );
    let earth_offset = camera_pos - EARTH_CENTER;
    let earth_dot_v = dot( earth_offset, view_dir );
    let ray_center_sq = dot( earth_offset, earth_offset ) - earth_dot_v * earth_dot_v;
    let ground_discriminant = EARTH.bottom_radius * EARTH.bottom_radius - ray_center_sq;
    if ( ground_discriminant >= 0.f ) {
        let ground_distance = -earth_dot_v - sqrt( ground_discriminant );

        if ( ground_distance > 0.f ) {
            let point = camera_pos + view_dir * ground_distance;
            let normal = normalize( point - EARTH_CENTER );
            let irradiance
                = get_sun_and_sky_irradiance( EARTH, point - EARTH_CENTER, normal, sun_dir );

            ground_radiance = GROUND_ALBEDO * INV_PI * irradiance.sun + irradiance.sky;

            let scatter = get_sky_radiance_to_point(
                EARTH, camera_pos - EARTH_CENTER, point - EARTH_CENTER, sun_dir );

            ground_radiance *= scatter.transmittance + scatter.radiance;
            ground_alpha = 1.f;
        }
    }

    let sky = get_sky_radiance( EARTH, camera_pos - EARTH_CENTER, view_dir, sun_dir );

    var radiance = sky.radiance;
    if ( dot( view_dir, sun_dir ) > SUN_SIZE ) {
        let solar_radiance
            = EARTH.solar_irradiance / ( PI * EARTH.sun_angular_radius * EARTH.sun_angular_radius );
        radiance += sky.transmittance * solar_radiance;
    }

    radiance = lerp( radiance, ground_radiance, ground_alpha );

    // Apply tone mapping via Reinhard and sRGB gamma correction
    let color = pow( float3( 1.f ) - exp( -radiance * RADIANCE_EXPOSURE ), GAMMA_FACTOR );
    return float4( color, 1.f );
}


