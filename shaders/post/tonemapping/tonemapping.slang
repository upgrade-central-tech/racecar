layout( binding = 0, set = 0 ) Texture2D<float4> input;
layout( binding = 1, set = 0 ) RWTexture2D<float4> output;

/// In Gran Turismo, 1.0f in the linear frame-buffer space corresponds to
/// `REFERENCE_LUMINANCE` cd/m^2 of physical luminance (typically 100 cd/m^2).
static const float REFERENCE_LUMINANCE = 100.f;

/// Defines the SDR reference white level used in GT7's tone mapping (typically 250 nits).
static const float GT_SDR_PAPER_WHITE = 250.f;

/// To match the sRGB range, the tonemapped values are scaled using this factor.
static const float SDR_CORRECTION_FACTOR = REFERENCE_LUMINANCE / GT_SDR_PAPER_WHITE;

/// Scale factor for exponent.
static const float JZAZBZ_EXPONENT_SCALE_FACTOR = 1.7f;

// Default parameters for applying tonemapping
static const float BLEND_RATIO = 0.6f;
static const float FADE_START = 0.98f;
static const float FADE_END = 1.16f;

namespace jzazbz {

func inverse_eotf_st_2084( v: float, exponent_scale_factor: float )->float
{
    let m1 = 0.1593017578125f;
    let m2 = 78.84375f * exponent_scale_factor;
    let c1 = 0.8359375f;
    let c2 = 18.8515625f;
    let c3 = 18.6875f;
    let pq_c = 10000.f;

    // Convert the framebuffer linear scale into absolute luminance (cd/m^2)
    let physical = framebuffer_value_to_physical_value( v );
    let y = physical / pq_c; // Normalize for the ST-2084 curve

    let ym = pow( y, m1 );
    return exp2( m2 * ( log2( c1 + c2 * ym ) - log2( 1.f + c3 * ym ) ) );
}

func eotf_st_2084( n: float, exponent_scale_factor: float )->float
{
    let n_new = clamp( n, 0.f, 1.f );

    // Base functions from SMPTE ST 2084:2014
    // Converts from normalized PQ (0-1) to absolute luminance in cd/m^2 (linear light)
    // Assumes float input; does not handle integer encoding (Annex)
    // Assumes full-range signal (0-1)
    let m1 = 0.1593017578125f; // (2610 / 4096) / 4
    let m2 = 78.84375f * exponent_scale_factor; // (2523 / 4096) * 128
    let c1 = 0.8359375f; // 3424 / 4096
    let c2 = 18.8515625f; // (2413 / 4096) * 32
    let c3 = 18.6875f; // (2392 / 4096) * 32
    let pq_c = 10000.f; // Maximum luminance supported by PQ (cd/m^2)

    // Does not handle signal range from 2084 - assumes full range (0-1)
    let np = pow( n, 1.f / m2 );
    var l = max( np - c1, 0.f );
    l = l / ( c2 - c3 * np );
    l = pow( l, 1.f / m1 );

    // Convert absolute luminance (cd/m^2) into the framebuffer linear scale
    return physical_value_to_framebuffer_value( l * pq_c );
}

/// Input: linear Rec.2020.
func from_rgb( float3 rgb )->float3
{
    let l = rgb[0] * 0.530004f + rgb[1] * 0.355704f + rgb[2] * 0.086090f;
    let m = rgb[0] * 0.289388f + rgb[1] * 0.525395f + rgb[2] * 0.157481f;
    let s = rgb[0] * 0.091098f + rgb[1] * 0.147588f + rgb[2] * 0.734234f;

    let lPQ = inverse_eotf_st_2084( l, JZAZBZ_EXPONENT_SCALE_FACTOR );
    let mPQ = inverse_eotf_st_2084( m, JZAZBZ_EXPONENT_SCALE_FACTOR );
    let sPQ = inverse_eotf_st_2084( s, JZAZBZ_EXPONENT_SCALE_FACTOR );

    let iz = 0.5f * lPQ + 0.5f * mPQ;

    var jab = float3( 0.f );
    jab[0] = ( 0.44f * iz ) / ( 1.0f - 0.56f * iz ) - 1.6295499532821566e-11f;
    jab[1] = 3.524000f * lPQ - 4.066708f * mPQ + 0.542708f * sPQ;
    jab[2] = 0.199076f * lPQ + 1.096799f * mPQ - 1.295875f * sPQ;

    return jab;
}

/// Output: linear Rec.2020.
func to_rgb( float3 jab )->float3
{
    let jz = jab[0] + 1.6295499532821566e-11f;
    let iz = jz / ( 0.44f + 0.56f * jz );
    let a = jab[1];
    let b = jab[2];

    let l = iz + a * 1.386050432715393e-1f + b * 5.804731615611869e-2f;
    let m = iz + a * -1.386050432715393e-1f + b * -5.804731615611869e-2f;
    let s = iz + a * -9.601924202631895e-2f + b * -8.118918960560390e-1f;

    let lLin = eotf_st_2084( l, JZAZBZ_EXPONENT_SCALE_FACTOR );
    let mLin = eotf_st_2084( m, JZAZBZ_EXPONENT_SCALE_FACTOR );
    let sLin = eotf_st_2084( s, JZAZBZ_EXPONENT_SCALE_FACTOR );

    var rgb = float3( 0.f );
    rgb[0] = lLin * 2.990669f + mLin * -2.049742f + sLin * 0.088977f;
    rgb[1] = lLin * -1.634525f + mLin * 3.145627f + sLin * -0.483037f;
    rgb[2] = lLin * -0.042505f + mLin * -0.377983f + sLin * 1.448019f;

    return rgb;
}

}

/// Converts linear framebuffer value to physical luminance (cd/m^2)
/// where 1.f corresponds to `REFERENCE_LUMINANCE` (e.g. 100 cd/m^2).
func framebuffer_value_to_physical_value( fb_value: float )->float
{
    return fb_value * REFERENCE_LUMINANCE;
}

/// Converts physical luminance (cd/m^2) to a linear framebuffer value,
/// where 1.f corresponds to `REFERENCE_LUMINANCE` (e.g. 100 cd/m^2).
func physical_value_to_framebuffer_value( physical: float )->float
{
    return physical / REFERENCE_LUMINANCE;
}

func chroma_curve( x: float, a: float, b: float )->float { return 1.f - smoothstep( x, a, b ); }

struct GTTonemappingCurveV2 {
    float peak_intensity_;
    float alpha_;
    float midpoint_;
    float linear_section_;
    float toe_strength_;
    float k_a_;
    float k_b_;
    float k_c_;

    __init( float monitor_intensity, float alpha, float gray_point, float linear_section,
        float toe_strength )
    {
        peak_intensity_ = monitor_intensity;
        alpha_ = alpha;
        midpoint_ = gray_point;
        linear_section_ = linear_section;
        toe_strength_ = toe_strength;

        // Pre-compute constants for the shoulder region
        float k = ( linear_section_ - 1.f ) / ( alpha_ - 1.f );
        k_a_ = peak_intensity_ * linear_section_ + peak_intensity_ * k;
        k_b_ = -peak_intensity_ * k * exp( linear_section_ / k );
        k_c_ = -1.f / ( k * peak_intensity_ );
    }

    func evaluate( x: float )->float
    {
        if ( x < 0.f ) {
            return 0.f;
        }

        if ( x < linear_section_ * peak_intensity_ ) {
            let weight_linear = smoothstep( x, 0.f, midpoint_ );
            let weight_toe = 1.f - weight_linear;
            let toe_mapped = midpoint_ * pow( x / midpoint_, toe_strength_ );

            return weight_toe * toe_mapped + weight_linear * x;
        } else {
            // Shoulder mapping for highlights
            let shoulder = k_a_ + k_b_ * exp( x * k_c_ );

            return shoulder;
        }
    }
}

/// Input: linear Rec.2020 RGB (framebuffer values), output is tonemapped RGB (we currently only
/// support SDR mode, which maps values to `[0, 1]`).
func apply_tonemapping( rgb: float3, in GTTonemappingCurveV2 curve,
    framebuffer_luminance_target: float, framebuffer_luminance_target_ucs: float )
    ->float3
{
    // Convert to UCS to separate luminance and chroma
    let ucs = jzazbz::from_rgb( rgb );

    // Per-channel tonemapping ("skewed" color).
    let skewed_rgb
        = float3( curve.evaluate( rgb.r ), curve.evaluate( rgb.g ), curve.evaluate( rgb.b ) );
    let skewed_ucs = jzazbz::from_rgb( skewed_rgb );

    let chroma_scale
        = chroma_curve( ucs[0] / framebuffer_luminance_target_ucs, FADE_START, FADE_END );
    let scaled_ucs = float3( skewed_ucs[0], // Luminance from skewed color
        ucs[1] * chroma_scale, // Scaled chroma components
        ucs[2] * chroma_scale );

    let scaled_rgb = jzazbz::to_rgb( scaled_ucs );

    // Final blend between per-channel and UCS-scaled results
    var output = float3( 0.f );
    for ( var i = 0; i < 3; ++i ) {
        let blended = ( 1.f - BLEND_RATIO ) * skewed_rgb[i] + BLEND_RATIO * scaled_rgb[i];

        // We're using SDR, so apply the correction factor (in HDR mode, the factor is simply 1.f)
        output[i] = SDR_CORRECTION_FACTOR * min( blended, framebuffer_luminance_target );
    }

    return output;
}

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
func cs_main( uint2 thread_id: SV_DispatchThreadID )->void
{
    let pixel = thread_id.xy;
    let scene = input.Load( uint3( pixel, 0 ) ).rgb;

    let framebuffer_luminance_target = physical_value_to_framebuffer_value( GT_SDR_PAPER_WHITE );
    let curve = GTTonemappingCurveV2( framebuffer_luminance_target, 0.25f, 0.538f, 0.444f, 1.28f );

    let rgb = float3( framebuffer_luminance_target );
    let ucs = jzazbz::from_rgb( rgb );
    let framebuffer_luminance_target_ucs = ucs.r; // Use the first UCS component (Jz) as luminance

    output[pixel] = float4( apply_tonemapping( scene, curve, framebuffer_luminance_target,
                                framebuffer_luminance_target_ucs ),
        1.f );
}


