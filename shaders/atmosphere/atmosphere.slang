module atmosphere;

__include common;

/// Stores the result of atmospheric scattering.
struct SkySample {
    float3 radiance; /// In-scattered light, i.e. the sky glow
    float3 transmittance; /// Ratio of light transmitted through the atmosphere
};

func get_sky_radiance( atmosphere: Parameters, camera: float3, view_ray: float3,
    shadow_length: float, sun_dir: float3 )
    ->SkySample
{
    return { float3( 0.f ), float3( 1.f ) };
}


