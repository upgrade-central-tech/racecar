module constants;

public struct DensityProfileLayer {
    public float width;
    public float exp_term;
    public float exp_scale;
    public float linear_term;
    public float constant_term;
};

public struct DensityProfile {
    /// Hardcoded to have two layers (lower and upper atmosphere)
    public DensityProfileLayer layers[2];
};

public struct Parameters {
    public float3 solar_irradiance;
    public float sun_angular_radius;
    public float bottom_radius;
    public float top_radius;

    public DensityProfile rayleigh_density;
    public float3 rayleigh_scattering;

    public DensityProfile mie_density;
    public float3 mie_scattering;
    public float3 mie_extinction;
    public float mie_phase_function_g;

    public DensityProfile absorption_density;
    public float3 absorption_extinction;

    public float3 ground_albedo;
    public float mu_s_min;
};

/// Compile-time constant. Values are calibrated to match Earth's atmosphere.
///
/// - Bottom radius: 6360 km (Earth's radius)
/// - Top radius: 6420 km (60 km atmosphere thickness)
public static const Parameters EARTH = {
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

public static const int IRRADIANCE_LUT_W = 64;
public static const int IRRADIANCE_LUT_H = 16;

public static const int SCATTERING_LUT_R_SIZE = 32;
public static const int SCATTERING_LUT_MU_SIZE = 128;
public static const int SCATTERING_LUT_MU_S_SIZE = 32;
public static const int SCATTERING_LUT_NU_SIZE = 8;

public static const int TRANSMITTANCE_LUT_W = 256;
public static const int TRANSMITTANCE_LUT_H = 64;

public static const float PI = 3.14159265358979323846f;
public static const float INV_PI = 0.3183098861837906715377f;

/// Expressed in radians.
public static const float SUN_ANGULAR_RADIUS = 0.04f;
public static const float SUN_SIZE = cos( SUN_ANGULAR_RADIUS );

/// Earth radius is 6360 km, which we represent as height. This is a negative value because
/// everything is relative to the Earth's surface.
public static const float3 EARTH_CENTER = { 0.f, -6360.f, 0.f };


