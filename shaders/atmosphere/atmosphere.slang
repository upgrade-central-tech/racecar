module atmosphere;

__include common;
__include helpers;

/// Stores the result of atmospheric scattering.
public struct SkySample {
    public float3 radiance; ///< In-scattered light, i.e. the sky glow
    public float3 transmittance; ///< Ratio of light transmitted through the atmosphere
};

func rayleigh_phase_function( nu: float )->float
{
    let k = 3.f / ( 16.f * PI );
    return k * ( 1.f + nu * nu );
}

func mie_phase_function( g: float, nu: float )->float
{
    let k = 3.f / ( 8.f * PI ) * ( 1.f - g * g ) / ( 2.f + g * g );
    return k * ( 1.f + nu * nu ) / pow( 1.f + g * g - 2.f * g * nu, 1.5f );
}

/// Computes sky color along a view ray.
public func get_sky_radiance(
    in atms: Parameters, camera: float3, view_ray: float3, sun_dir: float3 )
    ->SkySample
{
    var local_camera = camera;
    var r = length( local_camera );
    var rmu = dot( local_camera, view_ray );
    let distance_to_top = -rmu - sqrt( rmu * rmu - r * r + atms.top_radius * atms.top_radius );

    // Handle camera being outside atmosphere
    if ( distance_to_top > 0.f ) {
        local_camera += view_ray * distance_to_top;
        r = atms.top_radius;
        rmu += distance_to_top;
    } else if ( r > atms.top_radius ) {
        return { float3( 0.f ), float3( 1.f ) };
    }

    let mu = rmu / r;
    let mu_s = dot( local_camera, sun_dir ) / r;
    let nu = dot( view_ray, sun_dir );
    let ray_r_mu_intersects_ground = ray_intersects_ground( atms, r, mu );

    var transmittance : float3;
    if ( ray_r_mu_intersects_ground ) {
        transmittance = float3( 0.f );
    } else {
        transmittance = sample_transmittance_lut( atms, r, mu );
    }

    let scatter_result = sample_scattering_lut( atms, r, mu, mu_s, nu, ray_r_mu_intersects_ground );
    let radiance = scatter_result.scattering * rayleigh_phase_function( nu )
        + scatter_result.single_mie * mie_phase_function( atms.mie_phase_function_g, nu );

    return { radiance, transmittance };
}

public func get_sky_color( float3 view_dir )->float3
{
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

            ground_radiance = GROUND_ALBEDO * INV_PI * irradiance.sun + irradiance.sky;

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

    radiance = lerp( radiance, ground_radiance, ground_alpha );

    // Apply tone mapping via Reinhard and sRGB gamma correction
    let color = pow( float3( 1.f ) - exp( -radiance / white_point * exposure ), GAMMA_FACTOR );

    return color;
}

