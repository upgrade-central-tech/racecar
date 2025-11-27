//=======================================================================================
// Spherical Harmonics
//=======================================================================================

// https://patapom.com/blog/SHPortal/
float3 eval_SH( SHData data, float3 direction )
{
    float x = direction.x;
    float y = direction.y;
    float z = direction.z;

    float c1 = 0.42904276540489171563379376569857;
    float c2 = 0.51166335397324424423977581244463;
    float c3 = 0.24770795610037568833406429782001;
    float c4 = 0.88622692545275801364908374167057;
    float c5 = 0.247708;

#if 1
    float3 L00 = float3( data.coeff0.rgb );
    float3 L1n1 = float3( data.coeff0.a, data.coeff1.rg );
    float3 L10 = float3( data.coeff1.ba, data.coeff2.r );
    float3 L11 = float3( data.coeff2.gba );
    float3 L2n2 = float3( data.coeff3.rgb );
    float3 L2n1 = float3( data.coeff3.a, data.coeff4.rg );
    float3 L20 = float3( data.coeff4.ba, data.coeff5.r );
    float3 L21 = float3( data.coeff5.gba );
    float3 L22 = float3( data.coeff6.rgb );
#endif
#if 0
    float3 L00 = float3( 0.7953949, 0.4405923, 0.5459412 ); // float3( data.coeff0.rgb );
    float3 L1n1 = float3( 0.3981450, 0.3526911, 0.6097158 ); // float3( data.coeff0.a, data.coeff1.rg );
    float3 L10 = float3( -0.3424573, -0.1838151, -0.2715583 ); // float3( data.coeff1.ba, data.coeff2.r );
    float3 L11 = float3( -0.2944621, -0.0560606, 0.0095193 ); // float3( data.coeff2.gba );
    float3 L2n2 = float3( -0.1123051, -0.0513088, -0.1232869 ); // float3( data.coeff3.rgb );
    float3 L2n1 = float3( -0.2645007, -0.2257996, -0.4785847 ); // float3( data.coeff3.a, data.coeff4.rg );
    float3 L20 = float3( -0.1569444, -0.0954703, -0.1485053 ); // float3( data.coeff4.ba, data.coeff5.r );
    float3 L21 = float3( 0.5646247, 0.2161586, 0.1402643 ); // float3( data.coeff5.gba );
    float3 L22 = float3( 0.2137442, -0.0547578, -0.3061700 ); // float3( data.coeff6.rgb );
#endif
#if 0
    float3 L00 = irradiance_SH.Sample( point_sampler, float2( 0, 0 ) ).rgb;
    float3 L1n1 = irradiance_SH.Sample( point_sampler, float2( 1, 0 ) ).rgb;
    float3 L10 = irradiance_SH.Sample( point_sampler, float2( 2, 0 ) ).rgb;
    float3 L11 = irradiance_SH.Sample( point_sampler, float2( 3, 0 ) ).rgb;
    float3 L2n2 = irradiance_SH.Sample( point_sampler, float2( 4, 0 ) ).rgb;
    float3 L2n1 = irradiance_SH.Sample( point_sampler, float2( 5, 0 ) ).rgb;
    float3 L20 = irradiance_SH.Sample( point_sampler, float2( 6, 0 ) ).rgb;
    float3 L21 = irradiance_SH.Sample( point_sampler, float2( 7, 0 ) ).rgb;
    float3 L22 = irradiance_SH.Sample( point_sampler, float2( 8, 0 ) ).rgb;
#endif
    return max( float3( 0.0f ),
        +c4 * L00 + c2 * 2.0f * ( L11 * x + L1n1 * y + L10 * z )
            + c1 * 2.0f * ( L2n2 * x * y + L21 * x * z + L2n1 * y * z )
            + c1 * L22 * ( x * x - y * y ) + c3 * L20 * ( 3.0f * ( z * z ) - 1.0f ) );
    // return ( c1 * L22 * ( x * x - y * y ) ) + ( c3 * L20 * ( z * z ) ) + ( c4 * L00 - c5 * L20 )
    //     + ( 2.0f * c1 * ( L2n2 * x * y + L21 * x * z + L2n1 * y * z ) )
    //     + ( 2.0f * c2 * ( L11 * x + L1n1 * y + L10 * z ) );
}


