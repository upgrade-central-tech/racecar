layout( binding = 0, set = 0 ) Texture2D<float4> src_color;
layout( binding = 1, set = 0 ) Texture2D<float4> src_data;
layout( binding = 2, set = 0 ) RWTexture2D<float4> dst_color;
layout( binding = 3, set = 0 ) RWTexture2D<float4> dst_data;

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

    float4 colors[4];
    float distances[4];
    float stencils[4];

    for ( uint i = 0; i < 4; ++i ) {
        uint2 offset = uint2( i & 1, i >> 1 );
        uint2 src_pixel = min( pixel * 2 + offset, src_size - uint2( 1 ) );
        int3 load_coord = int3( int2( src_pixel ), 0 );

        colors[i] = src_color.Load( load_coord );

        float4 data = src_data.Load( load_coord );
        distances[i] = data.r;
        stencils[i] = data.g;
    }

    float ref_stencil = 0.0f;
    float best_count = 0.0f;

    for ( uint i = 0; i < 4; ++i ) {
        if ( stencils[i] == 0.0f ) {
            continue;
        }

        float count = 0.0f;
        for ( uint j = 0; j < 4; ++j ) {
            count += ( stencils[j] == stencils[i] ) ? 1.0f : 0.0f;
        }

        if ( count > best_count ) {
            best_count = count;
            ref_stencil = stencils[i];
        }
    }

    float3 color_sum = float3( 0.0f );
    float alpha_sum = 0.0f;
    float match_count = 0.0f;

    float distance_sum = 0.0f;
    float hit_count = 0.0f;

    for ( uint i = 0; i < 4; ++i ) {
        if ( stencils[i] != ref_stencil ) {
            continue;
        }

        color_sum += colors[i].rgb;
        alpha_sum += colors[i].a;
        match_count += 1.0f;

        if ( colors[i].a > 0.0f ) {
            distance_sum += distances[i];
            hit_count += 1.0f;
        }
    }

    dst_color[pixel] = float4( color_sum, alpha_sum ) / match_count;
    dst_data[pixel] = float4(
        hit_count > 0.0f ? distance_sum / hit_count : -1.0f, ref_stencil, 0.0f, 0.0f );
}
