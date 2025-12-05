module cloud_utils;

public uint32_t wang32( uint32_t x )
{
    x = ( x ^ 61u ) ^ ( x >> 16 );
    x *= 9u;
    x ^= x >> 4;
    x *= 0x27d4eb2du;
    x ^= x >> 15;
    return x;
}

// Returns in range [0...1)
public float rand_float( float seed )
{
    // Mask bottom 24-bits
    return ( wang32( asuint( seed ) ) & 0x00FFFFFFu ) / 16777216.0;
}

public float rand_float2( float2 p )
{
    uint32_t seed = ( asuint( p.x ) ) * 73856093u ^ ( asuint( p.y ) ) * 19349663u;
    return rand_float( seed );
}

public float rand_float3( float3 p )
{
    uint32_t seed = ( asuint( p.x ) ) * 73856093u ^ ( asuint( p.y ) ) * 19349663u
        ^ ( asuint( p.z ) ) * 83492791u;
    return rand_float( seed );
}

public float3 rand_float3_float3( float3 p )
{
    uint32_t seed = ( asuint( p.x ) ) * 73856093u ^ ( asuint( p.y ) ) * 19349663u
        ^ ( asuint( p.z ) ) * 83492791u;
    float x = rand_float3( seed );
    float y = rand_float( x );
    float z = rand_float( y );
    return normalize( 2.f * float3( x, y, z ) - float3( 1.f ) );
}

public float perlin_noise3D( float3 p )
{
    float3 grid_cell = floor( p );
    float3 fractional = p - grid_cell;

    float3 w = fractional * fractional * ( 3. - 2. * fractional );

    float f000 = dot(
        rand_float3_float3( grid_cell + float3( .0, .0, 0. ) ), fractional - float3( .0, .0, 0. ) );
    float f010 = dot(
        rand_float3_float3( grid_cell + float3( .0, 1., 0. ) ), fractional - float3( .0, 1., 0. ) );
    float f100 = dot(
        rand_float3_float3( grid_cell + float3( 1., 0., 0. ) ), fractional - float3( 1., 0., 0. ) );
    float f110 = dot(
        rand_float3_float3( grid_cell + float3( 1., 1., 0. ) ), fractional - float3( 1., 1., 0. ) );
    float f001 = dot(
        rand_float3_float3( grid_cell + float3( .0, .0, 1. ) ), fractional - float3( .0, .0, 1. ) );
    float f011 = dot(
        rand_float3_float3( grid_cell + float3( .0, 1., 1. ) ), fractional - float3( .0, 1., 1. ) );
    float f101 = dot(
        rand_float3_float3( grid_cell + float3( 1., 0., 1. ) ), fractional - float3( 1., 0., 1. ) );
    float f111 = dot(
        rand_float3_float3( grid_cell + float3( 1., 1., 1. ) ), fractional - float3( 1., 1., 1. ) );

    float f00 = lerp( f000, f001, w.z );
    float f01 = lerp( f010, f011, w.z );
    float f10 = lerp( f100, f101, w.z );
    float f11 = lerp( f110, f111, w.z );

    float xm1 = lerp( f00, f10, w.x );
    float xm2 = lerp( f01, f11, w.x );

    float ym = lerp( xm1, xm2, w.y );
    return ym;
}

// fractal perlin noise
public float noise_sum3D( float3 p )
{
    p *= 1.; // Cell size
    // amplitude, noise_sum, total_sum
    float a = 1., r = 0., s = 0.;

    for ( int i = 0; i < 5; i++ ) {
        r += a * perlin_noise3D( p );
        s += a;
        p *= 2.;
        a *= .5;
    }

    return r / s;
}

public float worley3D( float3 p )
{
    float r = 3.0;
    float3 f = floor( p );
    float3 x = fract( p );
    for ( int i = -1; i <= 1; i++ ) {
        for ( int j = -1; j <= 1; j++ ) {
            for ( int k = -1; k <= 1; k++ ) {
                float3 q = float3( float( i ), float( j ), float( k ) );
                float3 v = q
                    + float3( rand_float3( ( q + f ) * 1.11 ), rand_float3( ( q + f ) * 1.14 ),
                        rand_float3( ( q + f ) * 1.17 ) )
                    - x;
                float d = dot( v, v );
                r = min( r, d );
            }
        }
    }
    return sqrt( r );
}

// fractal worley noise
public float noise_sum3D_Worley( float3 p )
{
    p *= 4.; // Cell size
    // amplitude, noise_sum, total_sum
    float a = 1., r = 0., s = 0.;

    for ( int i = 0; i < 5; i++ ) {
        r += a * worley3D( p );
        s += a;
        p *= 2.;
        a *= .5;
    }

    return r / s;
}


