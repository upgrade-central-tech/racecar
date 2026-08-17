layout( binding = 0, set = 0 ) Texture2D<float4> src_color;
layout( binding = 1, set = 0 ) Texture2D<float4> src_data;
layout( binding = 2, set = 0 ) RWTexture2D<float4> dst_color;
layout( binding = 3, set = 0 ) RWTexture2D<float4> dst_data;

static const float TENT_WEIGHTS[4] = { 1.0f, 3.0f, 3.0f, 1.0f };
static const float TENT_TOTAL_WEIGHT = 64.0f;

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void cs_reflection_mips( uint3 thread_id: SV_DispatchThreadID )
{
    uint2 pixel = thread_id.xy;

    uint2 dst_size;
    dst_color.GetDimensions( dst_size.x, dst_size.y );

    if ( pixel.x >= dst_size.x || pixel.y >= dst_size.y ) {
        return;
    }

    uint2 src_size;
    src_color.GetDimensions( src_size.x, src_size.y );

    float4 colors[16];
    float distances[16];
    float stencils[16];
    float coverages[16];
    float weights[16];

    for ( uint i = 0; i < 16; ++i ) {
        uint2 tap = uint2( i & 3, i >> 2 );
        int2 offset = int2( tap ) - int2( 1 );
        int2 src_pixel
            = clamp( int2( pixel * 2 ) + offset, int2( 0 ), int2( src_size ) - int2( 1 ) );
        int3 load_coord = int3( src_pixel, 0 );

        colors[i] = src_color.Load( load_coord );

        float4 data = src_data.Load( load_coord );
        distances[i] = data.r;
        stencils[i] = data.g;
        coverages[i] = data.b;

        weights[i] = TENT_WEIGHTS[tap.x] * TENT_WEIGHTS[tap.y];
    }

    float ref_stencil = 0.0f;
    float best_weight = 0.0f;

    for ( uint i = 0; i < 16; ++i ) {
        if ( stencils[i] == 0.0f ) {
            continue;
        }

        float stencil_weight = 0.0f;
        for ( uint j = 0; j < 16; ++j ) {
            stencil_weight += ( stencils[j] == stencils[i] ) ? weights[j] : 0.0f;
        }

        if ( stencil_weight > best_weight ) {
            best_weight = stencil_weight;
            ref_stencil = stencils[i];
        }
    }

    float3 color_sum = float3( 0.0f );
    float alpha_sum = 0.0f;
    float match_weight = 0.0f;

    float distance_sum = 0.0f;
    float hit_weight = 0.0f;

    float coverage_sum = 0.0f;

    for ( uint i = 0; i < 16; ++i ) {
        if ( stencils[i] != ref_stencil ) {
            continue;
        }

        float weight = weights[i];

        color_sum += colors[i].rgb * weight;
        alpha_sum += colors[i].a * weight;
        match_weight += weight;

        coverage_sum += coverages[i] * weight;

        if ( colors[i].a > 0.0f ) {
            distance_sum += distances[i] * weight;
            hit_weight += weight;
        }
    }

    dst_color[pixel] = float4( color_sum, alpha_sum ) / match_weight;
    dst_data[pixel] = float4( hit_weight > 0.0f ? distance_sum / hit_weight : -1.0f, ref_stencil,
        coverage_sum / TENT_TOTAL_WEIGHT, 0.0f );
}
