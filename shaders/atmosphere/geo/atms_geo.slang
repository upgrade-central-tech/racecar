#language slang 2026

import constants;
import helpers;

typealias SunSkyIrradiance = Tuple<float, float>;

func get_transmittance_to_sun(
    Texture2D<float4> transmittance_lut, SamplerState sampler, r: float, mu_s: float )
    ->float3
{
    // Compute horizon angle at this altitude
    let sin_theta_h = EARTH.bottom_radius / r;
    let cos_theta_h = -safe_sqrt( 1.f - sin_theta_h * sin_theta_h );

    // Multiply transmittance by smooth fadeout near horizon
    return sample_transmittance_lut( transmittance_lut, sampler, r, mu_s )
        * smoothstep( -sin_theta_h * EARTH.sun_angular_radius,
            sin_theta_h * EARTH.sun_angular_radius, mu_s - cos_theta_h );
}

[shader( "vertex" )]
func get_sun_and_sky_irradiance( Texture2D<float4> irradiance_lut,
    Texture2D<float4> transmittance_lut, SamplerState sampler, surface_point: float3,
    surface_normal: float3, sun_dir: float3 )
    ->SunSkyIrradiance
{
    let r = length( surface_point );
    let mu_s = dot( surface_point, sun_dir ) / r;

    // Map (r, mu_s) to irradiance texture coordinates
    let x_r = ( r - EARTH.bottom_radius ) / ( EARTH.top_radius - EARTH.bottom_radius );
    let x_mu_s = mu_s * 0.5f + 0.5f; // Map [-1,1] to [0,1]
    let uv = float2(
        normalize_to_uv( x_mu_s, IRRADIANCE_LUT_W ), normalize_to_uv( x_r, IRRADIANCE_LUT_H ) );
    let irradiance = irradiance_lut.Sample( sampler, uv ).rgb;

    // Sky irradiance with hemisphere weighting
    let sky = irradiance * ( 1.f + dot( surface_normal, surface_point ) / r ) * 0.5f;
    let sun = EARTH.solar_irradiance
        * get_transmittance_to_sun( transmittance_lut, sampler, r, mu_s )
        * max( dot( surface_normal, sun_dir ), 0.f );

    return ( 0.f, 0.f );
}


