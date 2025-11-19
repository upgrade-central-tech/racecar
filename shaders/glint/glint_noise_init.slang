#include "glint_noise_init.hlsl"

struct TextureInfo {
    uint2 dimensions;
}

layout( binding = 0, set = 0 ) RWTexture2D<float4> glint_noise_texture;
layout( binding = 1, set = 0 ) ConstantBuffer<TextureInfo> noise_output;

float4 sample_packed_noise( uint2 pixelCoord00, uint2 dimensions ) {
    // Hardcoded for fast testing
    uint seed = 1u;

    // Generate random numbers for this cell and the next ones in X and Y
    uint rngState00 = WangHash( CoordToFlatId( pixelCoord00 * 123 ) + dimensions.x * dimensions.y * seed );
    float u00 = RandXorshiftFloat( rngState00 );
    float g00 = InvCDF( RandXorshiftFloat( rngState00 ), GAUSSIAN_AVG, GAUSSIAN_STD );

    int2 pixelCoord01 = ( pixelCoord00 + int2( 0, 1 ) ) % dimensions;
    uint rngState01 = WangHash( CoordToFlatId( pixelCoord01 * 123 ) + dimensions.x * dimensions.y * seed );
    float u01 = RandXorshiftFloat( rngState01 );
    float g01 = InvCDF( RandXorshiftFloat( rngState01 ), GAUSSIAN_AVG, GAUSSIAN_STD );

    int2 pixelCoord10 = ( pixelCoord00 + int2( 1, 0 ) ) % dimensions;
    uint rngState10 = WangHash( CoordToFlatId( pixelCoord10 * 123 ) + dimensions.x * dimensions.y * seed );
    float u10 = RandXorshiftFloat( rngState10 );
    float g10 = InvCDF( RandXorshiftFloat( rngState10 ), GAUSSIAN_AVG, GAUSSIAN_STD );

    int2 pixelCoord11 = ( pixelCoord00 + int2( 1, 1 ) ) % dimensions;
    uint rngState11 = WangHash( CoordToFlatId( pixelCoord11 * 123 ) + dimensions.x * dimensions.y * seed );
    float u11 = RandXorshiftFloat( rngState11 );
    float g11 = InvCDF( RandXorshiftFloat( rngState11 ), GAUSSIAN_AVG, GAUSSIAN_STD );

    // Pack 8 values into 4
    return float4( PackFloats( u00, g00 ), PackFloats( u01, g01 ), PackFloats( u10, g10 ), PackFloats( u11, g11 ) );
}

[shader("compute")]
[numthreads( 8, 8, 1 )]
void generate_glint_noise( uint3 thread_id : SV_DispatchThreadID ) {
    glint_noise_texture[thread_id.xy] = sample_packed_noise( thread_id.xy, noise_output.dimensions );
}