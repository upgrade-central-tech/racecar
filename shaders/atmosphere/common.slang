implementing atmosphere;

struct DensityProfileLayer {
    float width;
    float exp_term;
    float exp_scale;
    float linear_term;
    float constant_term;
};

struct DensityProfile {
    DensityProfileLayer layers[2]; ///< Hardcoded to have two layers (lower and upper atmosphere)
};

struct Parameters {
    float3 solar_irradiance;
    float sun_angular_radius;
    float bottom_radius;
    float top_radius;

    DensityProfile rayleigh_density;
    float3 rayleigh_scattering;

    DensityProfile mie_density;
    float3 mie_scattering;
    float3 mie_extinction;
    float mie_phase_function_g;

    DensityProfile absorption_density;
    float3 absorption_extinction;

    float3 ground_albedo;
    float mu_s_min;
}

/// Compile-time constant. Values are calibrated to match Earth's atmosphere.
static const Parameters ATMOSPHERE_PARAMETERS = {
    float3( 1.474000f, 1.850400f, 1.911980f ),
    0.004675f,
    6360.f,
    6420.f,

    DensityProfile( {
        { 0.f, 0.f, 0.f, 0.f, 0.f },
        { 0.f, 1.f, -0.125f, 0.f, 0.f },
    } ),
    float3( 0.005802f, 0.013558f, 0.033100f ),

    DensityProfile( {
        { 0.f, 0.f, 0.f, 0.f, 0.f },
        { 0.f, 1.f, -0.833333f, 0.f, 0.f },
    } ),
    float3( 0.003996f, 0.003996f, 0.003996f ),
    float3( 0.004440f, 0.004440f, 0.004440f ),
    0.8f,

    DensityProfile( {
        { 25.f, 0.f, 0.f, 0.066667f, -0.666667f },
        { 0.f, 0.f, 0.f, -0.066667f, 2.666667f },
    } ),
    float3( 0.000650f, 0.001881f, 0.000085f ),

    float3( 0.1f, 0.1f, 0.1f ),
    -0.207912f,
};


