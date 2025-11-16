static const float PI = 3.14159265359;

//=======================================================================================
// NDF functions
//=======================================================================================
float DistributionGGX( float n_dot_h, float alpha )
{
    float alpha2 = max( alpha * alpha, 0.001f );

    float n_dot_h_2 = n_dot_h * n_dot_h;
    float denom = ( n_dot_h_2 ) * ( alpha2 - 1.0f ) + 1.0f;

    return alpha2 / ( PI * denom * denom );
}

float DistributionGTR( float alpha, float h_dot_l )
{
    float alpha_2 = alpha * alpha;
    return ( alpha_2 - 1.0f )
        / ( PI * log( alpha_2 ) * ( 1.0f + ( alpha_2 - 1.0f ) * ( h_dot_l * h_dot_l ) ) );
}

//=======================================================================================
// Geometry functions
//=======================================================================================
float GeometrySchlick( float theta, float k ) { return theta / ( theta * ( 1.0f - k ) + k ); }

float GeometrySmith( float n_dot_v, float n_dot_l, float alpha )
{
    float num = ( alpha + 1.0f );
    float k = ( num * num ) / 8.0f;

    return GeometrySchlick( n_dot_v, k ) * GeometrySchlick( n_dot_l, k );
}

//=======================================================================================
// Fresnel schlick
//=======================================================================================
float3 FresnelSchlick( float cos_theta, float3 F0 )
{
    return F0 + ( 1.0f - F0 ) * pow( 1.0f - cos_theta, 5.0f );
}

//=======================================================================================
// Normal mapping helper funcs
//=======================================================================================
float3 map_normals( float3 normal_map, float4 in_tangent, float3 in_normal )
{
    float3 normal = normalize( in_normal );
    float3 tangent = normalize( in_tangent.xyz );
    float3 bitangent = -cross( normal, tangent )
        * in_tangent.w; // I hardcoded this flip, but this shouldn't be the case
    float3x3 TBN = float3x3( tangent, bitangent, normal );

    // Map back to [-1, 1] from [0, 1]
    float3 normal_tangent = 2.0f * normal_map - 1.0f;

    return mul( transpose( TBN ), normal_tangent );
}

float3 world_to_local( float3 world_dir, float3 normal )
{
    float3 up = abs( normal.z ) < 0.999f ? float3( 0, 0, 1 ) : float3( 1, 0, 0 );
    float3 tangent = normalize( cross( up, normal ) );
    float3 bitangent = cross( normal, tangent );
    float3x3 TBN = float3x3( tangent, bitangent, normal );

    return mul( transpose( TBN ), world_dir );
}

//=======================================================================================
// Spherical Harmonics
//=======================================================================================

// https://patapom.com/blog/SHPortal/
float3 eval_SH( SHData data, float3 direction )
{
    float x = direction.x;
    float y = direction.y;
    float z = direction.z;

    float c1 = 0.429043;
    float c2 = 0.511664;
    float c3 = 0.743125;
    float c4 = 0.886227;
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
#else
    float3 L00 = float3( 0.79, 0.44, 0.54 ) * 0.886227f; // float3( data.coeff0.rgb );
    float3 L1n1 = float3( 0.39, 0.35, 0.60 ) * 1.023326f; // float3( data.coeff0.a, data.coeff1.rg );
    float3 L10 = float3( -0.34, -0.18, -0.27 ) * 1.023326f; // float3( data.coeff1.ba, data.coeff2.r );
    float3 L11 = float3( -0.29, -0.06, 0.01 ) * 1.023326f; // float3( data.coeff2.gba );
    float3 L2n2 = float3( -0.11, -0.05, -0.12 ) * 0.247708f; // float3( data.coeff3.rgb );
    float3 L2n1 = float3( -0.26, -0.22, -0.47 ) * 0.247708f; // float3( data.coeff3.a, data.coeff4.rg );
    float3 L20 = float3( -0.16, -0.09, -0.15 ) * 0.247708f; // float3( data.coeff4.ba, data.coeff5.r );
    float3 L21 = float3( 0.56, 0.21, 0.14 ) * 0.247708f; // float3( data.coeff5.gba );
    float3 L22 = float3( 0.21, -0.05, -0.30 ) * 0.247708f; // float3( data.coeff6.rgb );
#endif

    return ( c1 * L22 * ( x * x - y * y ) ) + ( c3 * L20 * ( z * z ) ) + ( c4 * L00 - c5 * L20 )
        + ( 2.0f * c1 * ( L2n2 * x * y + L21 * x * z + L2n1 * y * z ) )
        + ( 2.0f * c2 * ( L11 * x + L1n1 * y + L10 * z ) );
}


