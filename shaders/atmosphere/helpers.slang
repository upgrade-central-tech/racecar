implementing atmosphere;

__include common;

import bindings;

struct ScatteringResult {
    float3 scattering;
    float3 single_mie;
};

func safe_sqrt( value: float )->float { return sqrt( max( value, 0.f ) ); }

/// Maps `[0, 1]` to texture space and centers samples in texel centers.
func normalize_to_uv( x: float, texture_size: int )->float
{
    return 0.5f / (float)texture_size + x * ( 1.f - 1.f / (float)texture_size );
}

func ray_distance_to_atmosphere_top( in atms: Parameters, r: float, mu: float )->float
{
    let discriminant = r * r * ( mu * mu - 1.f ) + atms.top_radius * atms.top_radius;
    return max( 0.f, -r * mu + safe_sqrt( discriminant ) );
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

func sample_transmittance_lut( in atms: Parameters, r: float, mu: float )->float3
{
    // Map (r, mu) to texture UV coordinates
    let H = sqrt( atms.top_radius * atms.top_radius - atms.bottom_radius * atms.bottom_radius );
    let rho = safe_sqrt( r * r - atms.bottom_radius * atms.bottom_radius );
    let d_min = atms.top_radius - r;
    let d_max = rho + H;
    let x_mu = ( ray_distance_to_atmosphere_top( atms, r, mu ) - d_min ) / ( d_max - d_min );
    let x_r = rho / H;
    let uv = float2(
        normalize_to_uv( x_mu, TRANSMITTANCE_LUT_H ), normalize_to_uv( x_r, TRANSMITTANCE_LUT_H ) );

    return transmittance_lut.Sample( sampler, uv ).rgb;
}

func sample_scattering_lut( in atms: Parameters, r: float, mu: float, mu_s: float, nu: float,
    ray_r_mu_intersects_ground: bool )
    ->ScatteringResult
{
    let uvwz = map_to_scattering_texture( atms, r, mu, mu_s, nu, ray_r_mu_intersects_ground );

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

func ray_intersects_ground( in atms: Parameters, r: float, mu: float )->bool
{
    return mu < 0.f
        && ( ( r * r * ( mu * mu - 1.f ) + atms.bottom_radius * atms.bottom_radius ) >= 0.f );
}


