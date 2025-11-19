module atmosphere;

__include common;
__include helpers;

/// Stores the result of atmospheric scattering.
public struct SkySample {
    public float3 radiance; ///< In-scattered light, i.e. the sky glow
    public float3 transmittance; ///< Ratio of light transmitted through the atmosphere
};

public struct SunSkyIrradiance {
    public float3 sun;
    public float3 sky;
};

struct CombinedScatteringResult {
    float3 scattering;
    float3 single_mie;
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

func extrapolate_single_mie( in atms: Parameters, scattering: float4 )->float3
{
    if ( scattering.x <= 0.f ) {
        return float3( 0.f );
    }

    return scattering.xyz * scattering.w / scattering.x
        * ( atms.rayleigh_scattering.x / atms.mie_scattering.x )
        * ( atms.mie_scattering / atms.rayleigh_scattering );
}

public func get_sky_radiance(
    in atms: Parameters, camera: float3, view_ray: float3, sun_dir: float3 )
    ->SkySample
{
    var local_camera = camera;
    var r = length( local_camera );
    var rmu = dot( local_camera, view_ray );

    let top_radius = atms.top_radius;
    let distance_to_top = -rmu - sqrt( rmu * rmu - r * r + top_radius * top_radius );

    // Handle camera being outside atmosphere
    if ( distance_to_top > 0.f ) {
        local_camera = local_camera + view_ray * distance_to_top;
        r = top_radius;
        rmu += distance_to_top;
    } else if ( r > top_radius ) {
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

/// Samples the irradiance LUT. Returns the total ground irradiance (sum of sun + sky light) at
/// `altitude` when the sun is at zenith angle `mu_s`.
func get_irradiance( in atms: Parameters, altitude: float, mu_s: float )->float3
{
    // Map (r, mu_s) to irradiance texture coordinates
    let x_r = ( altitude - atms.bottom_radius ) / ( atms.top_radius - atms.bottom_radius );
    let x_mu_s = mu_s * 0.5f + 0.5f;
    let uv = float2(
        normalize_to_uv( x_mu_s, IRRADIANCE_LUT_W ), normalize_to_uv( x_r, IRRADIANCE_LUT_H ) );

    return irradiance_lut.Sample( sampler, uv ).rgb;
}

/// Computes atmospheric transmittance to the sun. Returns the fraction of sunlight that reaches a
/// point at altitude `r`. It accounts for the atmospheric absorption along the path to the sun.
///
/// Also, we use `smoothstep` to smoothly transition as the sun sets/rises.
func get_transmittance_to_sun( in atms: Parameters, r: float, mu_s: float )->float3
{
    // Compute horizon angle at this altitude
    let sin_theta_h = atms.bottom_radius / r;
    let cos_theta_h = -sqrt( max( 1.f - sin_theta_h * sin_theta_h, 0.f ) );

    return sample_transmittance_lut( atms, r, mu_s )
        * smoothstep( -sin_theta_h * atms.sun_angular_radius, sin_theta_h * atms.sun_angular_radius,
            mu_s - cos_theta_h );
}

/// Returns the total irradiance at a surface point from two sources:
///
/// 1. Direct sunlight (with atmospheric attenuation)
/// 2. Diffuse skylight (light scattered from the sky dome)
///
/// The sky irradiance uses a hemisphere integral approximation, by multiplying by (1 + dot(normal,
/// zenith)) / 2. This is a simple approximation of integrating over the visible hemisphere.
///
/// Assumes `normal` and `sun_dir` is already normalized.
public func get_sun_and_sky_irradiance(
    in atms: Parameters, surface_point: float3, normal: float3, sun_dir: float3 )
    ->SunSkyIrradiance
{
    let r = length( surface_point );
    let mu_s = dot( surface_point, sun_dir ) / r;

    // Sky irradiance with hemisphere weighting
    let sky = get_irradiance( atms, r, mu_s ) * ( 1.f + dot( normal, surface_point ) / r ) * 0.5f;

    // Direct sun irradiance with cosine law
    let sun = atms.solar_irradiance * get_transmittance_to_sun( atms, r, mu_s )
        * max( dot( normal, sun_dir ), 0.f );

    return { sun, sky };
}

/// Computes transmittance between two points.
///
/// Returns the fraction of light that survives traveling distance `d` along a ray from (`r`, `mu`).
/// Handles both ground-intersecting and non-intersecting rays.
func get_transmittance(
    in atms: Parameters, r: float, mu: float, d: float, ray_r_mu_intersects_ground: bool )
    ->float3
{
    // Compute endpoint of ray segment
    let r_d = clamp_to_radius( atms, sqrt( d * d + 2.f * r * mu * d + r * r ) );
    let mu_d = clamp( ( r * mu + d ) / r_d, -1.f, 1.f );

    // Ray hits ground: use ratio of transmittances looking down
    if ( ray_r_mu_intersects_ground ) {
        return min(
            sample_transmittance_lut( atms, r_d, -mu_d ) / sample_transmittance_lut( atms, r, -mu ),
            float3( 1.f ) );
    }

    // Ray doesn't hit ground: use ratio of transmittances looking up
    return min(
        sample_transmittance_lut( atms, r, mu ) / sample_transmittance_lut( atms, r_d, mu_d ),
        float3( 1.f ) );
}

public func get_sky_radiance_to_point(
    in atms: Parameters, camera: float3, surface_point: float3, sun_dir: float3 )
    ->SkySample
{
    let view_ray = normalize( surface_point - camera );
    var local_camera = camera;
    var r = length( local_camera );
    var rmu = dot( local_camera, view_ray );
    let distance_to_top = -rmu - sqrt( rmu * rmu - r * r + atms.top_radius * atms.top_radius );

    if ( distance_to_top > 0.f ) {
        local_camera += view_ray * distance_to_top;
        r = atms.top_radius;
        rmu += distance_to_top;
    }

    let mu = rmu / r;
    let mu_s = dot( local_camera, sun_dir ) / r;
    let nu = dot( view_ray, sun_dir );
    var d = length( surface_point - local_camera );

    let ray_r_mu_intersects_ground = ray_intersects_ground( atms, r, mu );
    let transmittance = get_transmittance( atms, r, mu, d, ray_r_mu_intersects_ground );
    var scatter_result = sample_scattering_lut( atms, r, mu, mu_s, nu, ray_r_mu_intersects_ground );

    let r_p = clamp_to_radius( atms, sqrt( d * d + 2.f * r * mu * d + r * r ) );
    let mu_p = ( r * mu + d ) / r_p;
    let mu_s_p = ( r * mu_s + d * nu ) / r_p;
    let scatter_p
        = sample_scattering_lut( atms, r_p, mu_p, mu_s_p, nu, ray_r_mu_intersects_ground );

    scatter_result.scattering -= transmittance * scatter_p.scattering;
    scatter_result.single_mie -= transmittance * scatter_p.single_mie;
    scatter_result.single_mie *= smoothstep( 0.f, 0.01f, mu_s );

    let radiance = scatter_result.scattering * rayleigh_phase_function( nu )
        + scatter_result.single_mie * mie_phase_function( atms.mie_phase_function_g, nu );

    return { radiance, transmittance };
}


