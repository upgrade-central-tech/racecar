module helpers;

import constants;

/// Maps `[0, 1]` to texture space and centers samples in texel centers.
public func normalize_to_uv( x: float, texture_size: int )->float
{
    return 0.5f / (float)texture_size + x * ( 1.f - 1.f / (float)texture_size );
}

public func safe_sqrt( value: float )->float { return sqrt( max( value, 0.f ) ); }

public func ray_distance_to_atmosphere_top( r: float, mu: float )->float
{
    let discriminant = r * r * ( mu * mu - 1.f ) + EARTH.top_radius * EARTH.top_radius;
    return max( 0.f, -r * mu + safe_sqrt( discriminant ) );
}

public func clamp_radius( r: float )->float
{
    return clamp( r, EARTH.bottom_radius, EARTH.top_radius );
}

internal func map_to_scattering_texture(
    r: float, mu: float, mu_s: float, nu: float, intersected_ground: bool )
    ->float4
{
    let H = sqrt( EARTH.top_radius * EARTH.top_radius - EARTH.bottom_radius * EARTH.bottom_radius );
    let rho = safe_sqrt( r * r - EARTH.bottom_radius * EARTH.bottom_radius );
    let u_r = normalize_to_uv( rho / H, SCATTERING_LUT_R_SIZE );
    let r_mu = r * mu;
    let discriminant = r_mu * r_mu - r * r + EARTH.bottom_radius * EARTH.bottom_radius;

    var u_mu : float;
    if ( intersected_ground ) {
        let d = -r_mu - safe_sqrt( discriminant );
        let d_min = r - EARTH.bottom_radius;
        let d_max = rho;

        var ratio = 0.f;
        if ( d_max != d_min ) {
            ratio = ( d - d_min ) / ( d_max - d_min );
        }

        u_mu = 0.5f - 0.5f * normalize_to_uv( ratio, SCATTERING_LUT_MU_SIZE / 2 );
    } else {
        let d = -r_mu + safe_sqrt( discriminant + H * H );
        let d_min = EARTH.top_radius - r;
        let d_max = rho + H;
        u_mu = 0.5f
            + 0.5f
                * normalize_to_uv( ( d - d_min ) / ( d_max - d_min ), SCATTERING_LUT_MU_SIZE / 2 );
    }

    let d = ray_distance_to_atmosphere_top( EARTH.bottom_radius, mu_s );
    let d_min = EARTH.top_radius - EARTH.bottom_radius;
    let d_max = H;
    let a = ( d - d_min ) / ( d_max - d_min );
    let D = ray_distance_to_atmosphere_top( EARTH.bottom_radius, EARTH.mu_s_min );
    let A = ( D - d_min ) / ( d_max - d_min );
    let u_mu_s = normalize_to_uv( max( 1.f - a / A, 0.f ) / ( 1.f + a ), SCATTERING_LUT_MU_S_SIZE );
    let u_nu = ( nu + 1.f ) * 0.5f;

    return { u_nu, u_mu_s, u_mu, u_r };
}

public func sample_transmittance_lut(
    Texture2D<float4> transmittance_lut, SamplerState sampler, r: float, mu: float )
    ->float3
{
    // Map (r, mu) to texture UV coordinates
    let H = sqrt( EARTH.top_radius * EARTH.top_radius - EARTH.bottom_radius * EARTH.bottom_radius );
    let rho = safe_sqrt( r * r - EARTH.bottom_radius * EARTH.bottom_radius );
    let d_min = EARTH.top_radius - r;
    let d_max = rho + H;
    let x_mu = ( ray_distance_to_atmosphere_top( r, mu ) - d_min ) / ( d_max - d_min );
    let x_r = rho / H;
    let uv = float2(
        normalize_to_uv( x_mu, TRANSMITTANCE_LUT_H ), normalize_to_uv( x_r, TRANSMITTANCE_LUT_H ) );

    return transmittance_lut.Sample( sampler, uv ).rgb;
}

internal func extrapolate_single_mie( scattering: float4 )->float3
{
    if ( scattering.x <= 0.f ) {
        return float3( 0.f );
    }

    return scattering.xyz * scattering.w / scattering.x
        * ( EARTH.rayleigh_scattering.x / EARTH.mie_scattering.x )
        * ( EARTH.mie_scattering / EARTH.rayleigh_scattering );
}

/// First tuple element is Rayleigh component, second element is Mie
public func sample_scattering_lut( Texture3D<float4> scattering_lut, SamplerState sampler, r: float,
    mu: float, mu_s: float, nu: float, intersects_ground: bool )
    ->Tuple<float3, float3>
{
    let uvwz = map_to_scattering_texture( r, mu, mu_s, nu, intersects_ground );

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
        extrapolate_single_mie( combined ),
    };
}

public func rayleigh_phase_function( nu: float )->float
{
    let k = 3.f / ( 16.f * PI );
    return k * ( 1.f + nu * nu );
}

public func mie_phase_function( g: float, nu: float )->float
{
    let k = 3.f / ( 8.f * PI ) * ( 1.f - g * g ) / ( 2.f + g * g );
    return k * ( 1.f + nu * nu ) / pow( 1.f + g * g - 2.f * g * nu, 1.5f );
}


