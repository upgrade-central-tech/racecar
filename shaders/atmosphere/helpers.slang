implementing atmosphere;

__include common;
__include utils;

import bindings;

func ray_distance_to_atmosphere_top( in atms: Parameters, r: float, mu: float )->float
{
    let discriminant = r * r * ( mu * mu - 1.f ) + atms.top_radius * atms.top_radius;
    return clamp_distance( -r * mu + safe_sqrt( discriminant ) );
}

/// Maps (r, mu) to texture UV coordinates.
func map_to_transmittance_texture( in atms: Parameters, r: float, mu: float )->float2
{
    let H = sqrt( atms.top_radius * atms.top_radius - atms.bottom_radius * atms.bottom_radius );
    let rho = safe_sqrt( r * r - atms.bottom_radius * atms.bottom_radius );
    let d = ray_distance_to_atmosphere_top( atms, r, mu );

    let d_min = atms.top_radius - r;
    let d_max = rho + H;
    let x_mu = ( d - d_min ) / ( d_max - d_min );

    let x_r = rho / H;

    return {
        normalize_to_uv( x_mu, TRANSMITTANCE_LUT_H ),
        normalize_to_uv( x_r, TRANSMITTANCE_LUT_H ),
    };
}

func sample_transmittance_lut( in atms: Parameters, r: float, mu: float )->float3
{
    let uv = map_to_transmittance_texture( atms, r, mu );
#ifdef COMPUTE_NO_SAMPLE
    return transmittance_lut.SampleLevel( sampler, uv, 0.0f ).rgb;
#else
    return transmittance_lut.Sample( sampler, uv ).rgb;
#endif
}

func map_to_scattering_texture( in atms: Parameters, r: float, mu: float, mu_s: float, nu: float,
    ray_r_mu_intersects_ground: bool )
    ->float4
{
    let H = sqrt( atms.top_radius * atms.top_radius - atms.bottom_radius * atms.bottom_radius );
    let rho = safe_sqrt( r * r - atms.bottom_radius * atms.bottom_radius );
    let u_r = normalize_to_uv( rho / H, SCATTERING_LUT_R_SIZE );

    var u_mu : float;
    let r_mu = r * mu;
    let discriminant = r_mu * r_mu - r * r + atms.bottom_radius * atms.bottom_radius;
    if ( ray_r_mu_intersects_ground ) {
        let d = -r_mu - safe_sqrt( discriminant );
        let d_min = r - atms.bottom_radius;
        let d_max = rho;

        var ratio : float;
        if ( d_max == d_min ) {
            ratio = 0.f;
        } else {
            ratio = ( d - d_min ) / ( d_max - d_min );
        }
        u_mu = 0.5f - 0.5f * normalize_to_uv( ratio, SCATTERING_LUT_MU_SIZE / 2 );
    } else {
        let d = -r_mu + safe_sqrt( discriminant + H * H );
        let d_min = atms.top_radius - r;
        let d_max = rho + H;
        u_mu = 0.5f
            + 0.5f
                * normalize_to_uv( ( d - d_min ) / ( d_max - d_min ), SCATTERING_LUT_MU_SIZE / 2 );
    }

    let d = ray_distance_to_atmosphere_top( atms, atms.bottom_radius, mu_s );
    let d_min = atms.top_radius - atms.bottom_radius;
    let d_max = H;
    let a = ( d - d_min ) / ( d_max - d_min );
    let D = ray_distance_to_atmosphere_top( atms, atms.bottom_radius, atms.mu_s_min );
    let A = ( D - d_min ) / ( d_max - d_min );
    let u_mu_s = normalize_to_uv( max( 1.f - a / A, 0.f ) / ( 1.f + a ), SCATTERING_LUT_MU_S_SIZE );
    let u_nu = ( nu + 1.f ) * 0.5f;

    return { u_nu, u_mu_s, u_mu, u_r };
}

func map_to_irradiance_texture( in atms: Parameters, r: float, mu_s: float )->float2
{
    let x_r = ( r - atms.bottom_radius ) / ( atms.top_radius - atms.bottom_radius );
    let x_mu_s = mu_s * 0.5f + 0.5f;

    return {
        normalize_to_uv( x_mu_s, IRRADIANCE_LUT_W ),
        normalize_to_uv( x_r, IRRADIANCE_LUT_H ),
    };
}


