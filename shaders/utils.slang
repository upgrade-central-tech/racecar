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