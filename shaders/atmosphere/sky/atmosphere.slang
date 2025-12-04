module atmosphere;

import helpers;
import bindings;
import constants;

struct ScatteringResult {
    float3 scattering;
    float3 single_mie;
};

func extrapolate_single_mie( in atms: Parameters, scattering: float4 )->float3
{
    if ( scattering.x <= 0.f ) {
        return float3( 0.f );
    }

    return scattering.xyz * scattering.w / scattering.x
        * ( atms.rayleigh_scattering.x / atms.mie_scattering.x )
        * ( atms.mie_scattering / atms.rayleigh_scattering );
}

func sample_scattering_lut( in atms: Parameters, r: float, mu: float, mu_s: float, nu: float )
    ->ScatteringResult
{
    let uvwz = map_to_scattering_texture( atms, r, mu, mu_s, nu );

    // Compute which two `nu` slices to sample and interpolation factor
    let tex_coord_x = uvwz.x * (float)( SCATTERING_LUT_NU_SIZE - 1 );
    let tex_x = floor( tex_coord_x );
    let lerp = tex_coord_x - tex_x;

    var uvw0 = float3( ( tex_x + uvwz.y ) / (float)SCATTERING_LUT_NU_SIZE, uvwz.z, uvwz.w );
    var uvw1 = float3( ( tex_x + 1.f + uvwz.y ) / (float)SCATTERING_LUT_NU_SIZE, uvwz.z, uvwz.w );
    let combined0 = scattering_lut.Sample( sampler, uvw0 );
    let combined1 = scattering_lut.Sample( sampler, uvw1 );
    let combined = combined0 * ( 1.f - lerp ) + combined1 * lerp;

    return {
        combined.rgb,
        extrapolate_single_mie( atms, combined ),
    };
}

func map_to_scattering_texture( in atms: Parameters, r: float, mu: float, mu_s: float, nu: float )
    ->float4
{
    let H = sqrt( atms.top_radius * atms.top_radius - atms.bottom_radius * atms.bottom_radius );
    let rho = safe_sqrt( r * r - atms.bottom_radius * atms.bottom_radius );
    let u_r = normalize_to_uv( rho / H, SCATTERING_LUT_R_SIZE );

    var u_mu : float;
    let r_mu = r * mu;
    let discriminant = r_mu * r_mu - r * r + atms.bottom_radius * atms.bottom_radius;
    var d = -r_mu + safe_sqrt( discriminant + H * H );
    var d_min = atms.top_radius - r;
    var d_max = rho + H;
    u_mu = 0.5f
        + 0.5f * normalize_to_uv( ( d - d_min ) / ( d_max - d_min ), SCATTERING_LUT_MU_SIZE / 2 );

    d = ray_distance_to_atmosphere_top( atms.bottom_radius, mu_s );
    d_min = atms.top_radius - atms.bottom_radius;
    d_max = H;
    let a = ( d - d_min ) / ( d_max - d_min );
    let D = ray_distance_to_atmosphere_top( atms.bottom_radius, atms.mu_s_min );
    let A = ( D - d_min ) / ( d_max - d_min );
    let u_mu_s = normalize_to_uv( max( 1.f - a / A, 0.f ) / ( 1.f + a ), SCATTERING_LUT_MU_S_SIZE );
    let u_nu = ( nu + 1.f ) * 0.5f;

    return { u_nu, u_mu_s, u_mu, u_r };
}

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
    let scatter_result = sample_scattering_lut( atms, r, mu, mu_s, nu );
    let radiance = scatter_result.scattering * rayleigh_phase_function( nu )
        + scatter_result.single_mie * mie_phase_function( atms.mie_phase_function_g, nu );

    let transmittance = sample_transmittance_lut( transmittance_lut, sampler, r, mu );

    return { radiance, transmittance };
}

public func get_sky_color( float3 view_dir )->float3
{
    let camera_pos = uniforms.camera_position;
    let sun_dir = normalize( uniforms.sun_direction );

    let sky = get_sky_radiance( EARTH, camera_pos - EARTH_CENTER, view_dir, sun_dir );
    var radiance = sky.radiance;

    // Add sun disk if looking toward the sun
    if ( dot( view_dir, sun_dir ) > SUN_SIZE ) {
        let solar_radiance
            = EARTH.solar_irradiance / ( PI * EARTH.sun_angular_radius * EARTH.sun_angular_radius );
        radiance += sky.transmittance * solar_radiance;
    }

    return radiance * uniforms.radiance_exposure;
}

public func get_rough_sky_color( float3 view_dir )->float3
{
    let camera_pos = uniforms.camera_position;
    let sun_dir = normalize( uniforms.sun_direction );

    let sky = get_sky_radiance( EARTH, camera_pos - EARTH_CENTER, view_dir, sun_dir );
    var radiance = sky.radiance;

    // Add sun disk if looking toward the sun
    if ( dot( view_dir, sun_dir ) > SUN_SIZE ) {
        let solar_radiance = EARTH.solar_irradiance
            / ( PI * EARTH.sun_angular_radius * EARTH.sun_angular_radius ) * 0.0001f;
        radiance += sky.transmittance * solar_radiance;
    }

    return radiance * uniforms.radiance_exposure;
}


