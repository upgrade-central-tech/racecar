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


