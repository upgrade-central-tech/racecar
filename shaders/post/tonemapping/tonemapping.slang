layout( binding = 0, set = 0 ) Texture2D<float4> input;
layout( binding = 1, set = 0 ) RWTexture2D<float4> output;

/// In Gran Turismo, 1.0f in the linear frame-buffer space corresponds to
/// `REFERENCE_LUMINANCE` cd/m^2 of physical luminance (typically 100 cd/m^2).
static const float REFERENCE_LUMINANCE = 100.f;

/// Defines the SDR reference white level used in GT7's tone mapping (typically 250 nits).
static const float GT_SDR_PAPER_WHITE = 250.f;

/// To match the sRGB range, the tonemapped values are scaled using this factor.
static const float SDR_CORRECTION_FACTOR = REFERENCE_LUMINANCE / GT_SDR_PAPER_WHITE;

/// Converts physical luminance (cd/m^2) to a linear framebuffer value,
/// where 1.f corresponds to `REFERENCE_LUMINANCE` (e.g. 100 cd/m^2).
constexpr func physical_value_to_framebuffer_value( physical: float )->float
{
    return physical / REFERENCE_LUMINANCE;
}

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

    func evaluate_curve( x: float )->float
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

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
func cs_main( uint2 thread_id: SV_DispatchThreadID )->void
{
    let pixel = thread_id.xy;
    let scene = input.Load( uint3( pixel, 0 ) ).rgb;

    let framebuffer_luminance_target = physical_value_to_framebuffer_value( GT_SDR_PAPER_WHITE );
    let curve = GTTonemappingCurveV2( framebuffer_luminance_target, 0.25f, 0.538f, 0.444f, 1.28f );
    let blend_ratio = 0.6f;
    let fade_start = 0.98f;
    let fade_end = 1.16f;

    output[pixel] = float4( 1.f - scene, 1.f );
}


