#language slang 2026

import constants;
import helpers;

typealias SunSkyIrradiance = Tuple<float, float>;

[shader( "vertex" )]
func get_sun_and_sky_irradiance( in Texture2D<float4> irradiance_lut, in SamplerState sampler,
    surface_point: float3, surface_normal: float3, sun_dir: float3 )
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

    return ( 0.f, 0.f );
}


