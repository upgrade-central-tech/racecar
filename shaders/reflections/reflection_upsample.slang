layout( binding = 0, set = 0 ) Texture2D<float4> src_color;
layout( binding = 1, set = 0 ) Texture2D<float4> src_data;
layout( binding = 2, set = 0 ) Texture2D<float4> dst_data;
layout( binding = 3, set = 0 ) RWTexture2D<float4> dst_color;

layout( binding = 0, set = 1 ) SamplerState linear_sampler;

static const float UPSAMPLE_BLEND = 0.5f;

static const float STENCIL_TOLERANCE = 0.5f;

static const float TENT_WEIGHTS[9] = {
    1.0f, 2.0f, 1.0f,
    2.0f, 4.0f, 2.0f,
    1.0f, 2.0f, 1.0f,
};

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void cs_reflection_upsample( uint3 thread_id: SV_DispatchThreadID )
{
    uint2 pixel = thread_id.xy;

    uint2 dst_size;
    dst_color.GetDimensions( dst_size.x, dst_size.y );

    if ( pixel.x >= dst_size.x || pixel.y >= dst_size.y ) {
        return;
    }

    float ref_stencil = dst_data.Load( int3( int2( pixel ), 0 ) ).g;

    uint2 src_size;
    src_color.GetDimensions( src_size.x, src_size.y );

    float2 uv = ( float2( pixel ) + 0.5f ) / float2( dst_size );
    float2 delta = 1.0f / float2( src_size );

    float3 color_sum = float3( 0.0f );
    float alpha_sum = 0.0f;
    float weight_sum = 0.0f;

    for ( uint i = 0; i < 9; ++i ) {
        float2 offset = float2( float( i % 3 ) - 1.0f, float( i / 3 ) - 1.0f );
        float2 tap_uv = uv + offset * delta;

        float tap_stencil = src_data.SampleLevel( linear_sampler, tap_uv, 0.0f ).g;
        if ( abs( tap_stencil - ref_stencil ) > STENCIL_TOLERANCE ) {
            continue;
        }

        float4 tap_color = src_color.SampleLevel( linear_sampler, tap_uv, 0.0f );
        float weight = TENT_WEIGHTS[i];

        color_sum += tap_color.rgb * weight;
        alpha_sum += tap_color.a * weight;
        weight_sum += weight;
    }

    if ( weight_sum == 0.0f ) {
        return;
    }

    float4 upsampled = float4( color_sum, alpha_sum ) / weight_sum;
    dst_color[pixel] = lerp( dst_color[pixel], upsampled, UPSAMPLE_BLEND );
}
