#include "cloud_utils.slang"

layout( binding = 0, set = 0 ) RWTexture3D<float> low_frequency_noise;
layout( binding = 1, set = 0 ) RWTexture3D<float> high_frequency_noise;
layout( binding = 2, set = 0 ) RWTexture2D<float> cumulus_map;

[shader("compute")]
[numthreads( 8, 8, 8 )]
void cs_generate_low_frequency( uint3 thread_id: SV_DispatchThreadID ) {
    float ndc_x = 2.0f * float( thread_id.x ) / 128.0f - 1.0f;
    float ndc_y = 2.0f * float( thread_id.y ) / 128.0f - 1.0f;
    float ndc_z = 2.0f * float( thread_id.z ) / 128.0f - 1.0f;
    float3 pos = float3( ndc_x, ndc_y, ndc_z );

    float worley = noise_sum3D_Worley( pos * 1.5 + 100.f );
    worley = smoothstep( 0.0, 0.8, worley ) - 0.2;

    float low_freq_perlin = noise_sum3D( pos * 1.5 + 100.f ) + 0.5;
    low_freq_perlin = 0.8 * smoothstep( 0.3, 0.6, low_freq_perlin ) + 0.2;
    float low_freq_noise = low_freq_perlin * ( 1.2 - worley ) * 0.5;
    low_freq_noise *= 0.5;
    low_freq_noise -= 0.25;

    low_frequency_noise[thread_id] = low_freq_noise;
}

[shader( "compute" )]
[numthreads( 8, 8, 8 )]
void cs_generate_high_frequency( uint3 thread_id: SV_DispatchThreadID ) {
    float ndc_x = 2.0f * float( thread_id.x ) / 128.0f - 1.0f;
    float ndc_y = 2.0f * float( thread_id.y ) / 128.0f - 1.0f;
    float ndc_z = 2.0f * float( thread_id.z ) / 128.0f - 1.0f;
    float3 pos = float3( ndc_x, ndc_y, ndc_z );

    float high_freq_noise = noise_sum3D_Worley( pos * 7 + 100.f );
    high_freq_noise = 0.8 - 0.7 * smoothstep( 0.0, 0.8, high_freq_noise );

    float noise = 1.0f;
    low_frequency_noise[thread_id] = noise;
}

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void cs_generate_cumulus( uint3 thread_id: SV_DispatchThreadID ) {
    uint2 pixel = thread_id.xy;

    float ndc_x = 2.0f * float( pixel.x ) / 1024.0f - 1.0f;
    float ndc_z = 2.0f * float( pixel.y ) / 1024.0f - 1.0f;

    // Apply scaling
    float2 input = float2( ndc_x, ndc_z ) * 2.0f + 100.0f;

    // Noise function magic
    float noise = noise_sum3D( float3( input, 1.0f ) );
    noise += 0.5f;

    cumulus_map[thread_id.xy] = smoothstep( 0.0f, 1.0f, noise );
}