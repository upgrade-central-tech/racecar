import constants;
import helpers;

typealias SunSkyIrradiance = Tuple<float3, float3>;
typealias RadianceTransmittance = Tuple<float3, float3>;

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

// [shader( "vertex" )]
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

    return makeTuple( sun, sky );
}

func get_transmittance( Texture2D<float4> transmittance_lut, SamplerState sampler, r: float,
    mu: float, d: float, intersects_ground: bool )
    ->float3
{
    // Compute endpoint of ray segment
    let r_d = clamp_radius( sqrt( d * d + 2.f * r * mu * d + r * r ) );
    let mu_d = clamp( ( r * mu + d ) / r_d, -1.f, 1.f );

    if ( intersects_ground ) {
        // Ray hits ground: use ratio of transmittances looking down
        return min( sample_transmittance_lut( transmittance_lut, sampler, r_d, -mu_d )
                / sample_transmittance_lut( transmittance_lut, sampler, r, -mu ),
            float3( 1.f ) );
    }

    // Ray doesn't hit ground: use ratio of transmittances looking up
    return min( sample_transmittance_lut( transmittance_lut, sampler, r, mu )
            / sample_transmittance_lut( transmittance_lut, sampler, r_d, mu_d ),
        float3( 1.f ) );
}

func get_sky_radiance_to_point( Texture2D<float4> transmittance_lut,
    Texture3D<float4> scattering_lut, SamplerState sampler, camera_pos: float3,
    surface_point: float3, sun_dir: float3, intersects_ground: bool )
    ->RadianceTransmittance
{
    let view_ray = normalize( surface_point - camera_pos );
    var local_camera = camera_pos;
    var r = length( local_camera );
    var rmu = dot( local_camera, view_ray );
    let distance_to_top = -rmu - sqrt( rmu * rmu - r * r + EARTH.top_radius * EARTH.top_radius );
    if ( distance_to_top > 0.f ) {
        local_camera = local_camera + view_ray * distance_to_top;
        r = EARTH.top_radius;
        rmu += distance_to_top;
    }

    let mu = rmu / r;
    let mu_s = dot( local_camera, sun_dir ) / r;
    let nu = dot( view_ray, sun_dir );
    let d = max( length( surface_point - local_camera ), 0.f );
    let transmittance = get_transmittance( transmittance_lut, sampler, r, mu, d, true );
    let scatter_result = sample_scattering_lut( scattering_lut, sampler, r, mu, mu_s, nu, true );

    let r_p = clamp_radius( sqrt( d * d + 2.f * r * mu * d + r * r ) );
    let mu_p = ( r * mu + d ) / r_p;
    let mu_s_p = ( r * mu_s + d * nu ) / r_p;
    let scatter_p = sample_scattering_lut( scattering_lut, sampler, r_p, mu_p, mu_s_p, nu, true );

    let scattering = scatter_result._0 - transmittance * scatter_p._0;
    var single_mie = scatter_result._1 - transmittance * scatter_p._1;
    single_mie *= smoothstep( 0.f, 0.01f, mu_s );

    let radiance = scattering * rayleigh_phase_function( nu )
        + single_mie * mie_phase_function( EARTH.mie_phase_function_g, nu );

    return makeTuple( radiance, transmittance );
}


