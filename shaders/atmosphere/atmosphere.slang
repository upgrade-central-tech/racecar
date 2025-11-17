module atmosphere;

__include common;
__include helpers;

/// Stores the result of atmospheric scattering.
struct SkySample {
    float3 radiance; /// In-scattered light, i.e. the sky glow
    float3 transmittance; /// Ratio of light transmitted through the atmosphere
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

func get_combined_scattering( in atms: Parameters, r: float, mu: float, mu_s: float, nu: float )
    ->CombinedScatteringResult
{
    let uvwz = map_to_scattering_texture( atms, r, mu, mu_s, nu );

    // Compute which two nu slices to sample and interpolation factor
    let tex_coord_x = uvwz.x * (float)( SCATTERING_LUT_NU_SIZE - 1 );
    let tex_x = floor( tex_coord_x );
    let lerp = tex_coord_x - tex_x;

    let uvw0 = float3( ( tex_x + uvwz.y ) / (float)SCATTERING_LUT_NU_SIZE, uvwz.z, uvwz.w );
    let uvw1 = float3( ( tex_x + 1.f + uvwz.y ) / (float)SCATTERING_LUT_NU_SIZE, uvwz.z, uvwz.w );
    let combined0 = scattering_lut.Sample( sampler, uvw0 );
    let combined1 = scattering_lut.Sample( sampler, uvw1 );
    let combined = combined0 * ( 1.f - lerp ) + combined1 * lerp;

    return {
        combined.rgb,
        extrapolate_single_mie( atms, combined ),
    };
}

func get_sky_radiance(
    in atms: Parameters, camera: float3, view_ray: float3, shadow_length: float, sun_dir: float3 )
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

    let transmittance = sample_transmittance_lut( atms, r, mu );
    let scatter_result = get_combined_scattering( atms, r, mu, mu_s, nu );
    let radiance = scatter_result.scattering * rayleigh_phase_function( nu )
        + scatter_result.single_mie * mie_phase_function( atms.mie_phase_function_g, nu );

    return { radiance, transmittance };
}


