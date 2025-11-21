#define HARDCODED_OCTAHEDRAL_SIZE 512.0f
#define COMPUTE_NO_SAMPLE 1

import atmosphere;

// In atmosphere, the bindings are already there and used for other things.
layout( binding = 0, set = 3 ) RWTexture2D<float4> octahedral_sky;

float3 oct_decode( float2 f )
{
    // Map from [0, 1] to [-1, 1]
    f = f * 2.0f - 1.0f;

    float3 n = float3( f.x, f.y, 1.0f - abs( f.x ) - abs( f.y ) );

    // Unfold the lower hemisphere
    if ( n.z < 0.0f )
        n.xy = ( 1.0f - abs( n.yx ) ) * sign( n.xy );

    return normalize( n );
}

[shader("compute")]
[numthreads( 8, 8, 8 )]
void cs_bake_atmosphere( uint3 thread_id: SV_DispatchThreadID ) {
    // Thread XY represents a direction.
    uint2 pixel = thread_id.xy;
    uint face = thread_id.z;

    float u = ( float( pixel.x ) + 0.5f ) / HARDCODED_OCTAHEDRAL_SIZE;
    float v = ( float( pixel.y ) + 0.5f ) / HARDCODED_OCTAHEDRAL_SIZE;

    float3 view_direction = oct_decode( float2( u, v ) );

    // Insert the work to actually derive a 3D global direction from pixel and face
    float3 sky_color = get_sky_color( view_direction );

    octahedral_sky[pixel] = float4( sky_color, 1.0f );
}