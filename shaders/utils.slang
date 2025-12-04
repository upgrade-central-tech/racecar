/// Has to be namespaced because we're defining `PI` somewhere else
namespace math {

public static const float PI = 3.14159265358979323846f;

}

//=======================================================================================
// BSDF Stuff
//=======================================================================================

//
struct GBuffer {
    float3 in_pos;
    float3 in_nor;
    float4 in_tangent;
    float2 in_uv;
    float2 ddx_uv;
    float2 ddy_uv;
}

struct Material {
    float roughness;
    float metallic;
    float clearcoat_roughness;
    float clearcoat_weight;
}

struct BSDF_Dots {
    // Essential dot products
    float n_dot_v;
    float n_dot_h;
    float n_dot_l;
    float h_dot_l;
    float v_dot_h;

    float clamped_n_dot_v;
    float clamped_n_dot_h;
    float clamped_n_dot_l;
    float clamped_h_dot_l;
    float clamped_v_dot_h;
}

BSDF_Dots get_dots( float3 normal, float3 view, float3 half, float3 light )
{
    // Essential dot products
    BSDF_Dots dots;
    dots.n_dot_v = ( dot( normal, view ) );
    dots.n_dot_h = ( dot( normal, half ) );
    dots.n_dot_l = ( dot( normal, light ) );
    dots.h_dot_l = ( dot( half, light ) );
    dots.v_dot_h = ( dot( view, half ) );

    dots.clamped_n_dot_v = max( dots.n_dot_v, 0.0f );
    dots.clamped_n_dot_h = max( dots.n_dot_h, 0.0f );
    dots.clamped_n_dot_l = max( dots.n_dot_l, 0.0f );
    dots.clamped_h_dot_l = max( dots.h_dot_l, 0.0f );
    dots.clamped_v_dot_h = max( dots.v_dot_h, 0.0f );

    return dots;
}

float2 oct_encode( float3 n )
{
    // Project the normal onto the octahedron
    n /= ( abs( n.x ) + abs( n.y ) + abs( n.z ) );

    // Fold the lower hemisphere
    if ( n.z < 0.0f )
        n.xy = ( 1.0f - abs( n.yx ) ) * sign( n.xy );

    // Map from [-1, 1] to [0, 1]
    return n.xy * 0.5f + 0.5f;
}

//=======================================================================================
// NDF functions
//=======================================================================================
// TODO: keep consistent case in this shader code
float DistributionGGX( float n_dot_h, float alpha )
{
    float alpha2 = max( alpha * alpha, 0.001f );

    float n_dot_h_2 = n_dot_h * n_dot_h;
    float denom = ( n_dot_h_2 ) * ( alpha2 - 1.0f ) + 1.0f;

    return alpha2 / ( math::PI * denom * denom );
}

float DistributionGTR( float alpha, float h_dot_l )
{
    float alpha_2 = alpha * alpha;
    return ( alpha_2 - 1.0f )
        / ( math::PI * log( alpha_2 ) * ( 1.0f + ( alpha_2 - 1.0f ) * ( h_dot_l * h_dot_l ) ) );
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
// Fresnel schlick roughness
//=======================================================================================
float3 FresnelSchlickRoughness( float cos_theta, float3 F0, float roughness )
{
    return F0
        + ( max( float3( 1.0f - roughness ), F0 ) - F0 )
        * pow( clamp( 1.0f - cos_theta, 0.0f, 1.0f ), 5.0f );
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
// Manual filtering helper functions
//=======================================================================================
float4 filter_float4( Texture2D<float4> texture, float2 world_uv )
{
    uint width, height;
    texture.GetDimensions( width, height );

    float2 texel_space = world_uv * float2( width, height );

    float2 texel0 = floor( texel_space - 0.5f );
    float2 subpixel = frac( texel_space - 0.5f );

    int2 i00 = int2( texel0 ); // tl
    int2 i10 = i00 + int2( 1, 0 ); // tr
    int2 i01 = i00 + int2( 0, 1 ); // bl
    int2 i11 = i00 + int2( 1, 1 ); // br

    float4 c00 = texture[i00];
    float4 c10 = texture[i10];
    float4 c01 = texture[i01];
    float4 c11 = texture[i11];

    float4 w
        = float4( ( 1.0f - subpixel.x ) * ( 1.0f - subpixel.y ), subpixel.x * ( 1.0f - subpixel.y ),
            ( 1.0f - subpixel.x ) * subpixel.y, subpixel.x * subpixel.y, );

    return c00 * w.x + c10 * w.y + c01 * w.z + c11 * w.w;
}

//=======================================================================================
// Depth functions
//=======================================================================================
float LinearDepth( float perspective_depth, float near, float far )
{
    return ( near * far ) / ( far - perspective_depth * ( far - near ) );
}

float LinearDepth01( float perspective_depth, float near, float far )
{
    float linear_depth = LinearDepth( perspective_depth, near, far );
    return ( linear_depth - near ) / ( far - near );
}

//=======================================================================================
// Misc. math
//=======================================================================================
float clamp_range( float input, float min, float max )
{
    return saturate( ( input - min ) / ( max - min ) );
}


