/// Has to be namespaced because we're defining `PI` somewhere else
namespace math {

public static const float PI = 3.14159265358979323846f;

}

//=======================================================================================
// BSDF Stuff
//=======================================================================================

// why is this in utils
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
    float v_dot_l;

    float clamped_n_dot_v;
    float clamped_n_dot_h;
    float clamped_n_dot_l;
    float clamped_h_dot_l;
    float clamped_v_dot_h;
    float clamped_v_dot_l;
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
    dots.v_dot_l = ( dot( view, light ) );

    dots.clamped_n_dot_v = max( dots.n_dot_v, 0.0f );
    dots.clamped_n_dot_h = max( dots.n_dot_h, 0.0f );
    dots.clamped_n_dot_l = max( dots.n_dot_l, 0.0f );
    dots.clamped_h_dot_l = max( dots.h_dot_l, 0.0f );
    dots.clamped_v_dot_h = max( dots.v_dot_h, 0.0f );
    dots.clamped_v_dot_l = max( dots.v_dot_l, 0.0f );

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

// Decima 2017 area circle
float get_max_n_dot_h_sq( float radius_tan, float n_dot_l, float n_dot_v, float v_dot_l )
{
    float radius_cos = rsqrt( 1.0f + radius_tan * radius_tan );

    float r_dot_l = 2.0f * n_dot_l * n_dot_v - v_dot_l;
    if ( r_dot_l >= radius_cos ) {
        return 1.0f;
    }

    float r_over_len_t = radius_cos * radius_tan * rsqrt( 1.0f - r_dot_l * r_dot_l );
    float n_dot_tr = r_over_len_t * ( n_dot_v - r_dot_l * n_dot_l );
    float v_dot_tr = r_over_len_t * ( 2.0f * n_dot_v * n_dot_v - 1.0f - r_dot_l * v_dot_l );

    // Decima maximization code
    float triple = sqrt( saturate( 1.0f - n_dot_l * n_dot_l - n_dot_v * n_dot_v - v_dot_l * v_dot_l
        + 2.0f * n_dot_l * n_dot_v * n_dot_l ) );
    float n_dot_br = r_over_len_t * triple;
    float v_dot_br = r_over_len_t * ( 2.0f * triple * n_dot_v );
    float n_dot_LVtr = n_dot_l * radius_cos + n_dot_v + n_dot_tr;
    float v_dot_LVtr = v_dot_l * radius_cos + 1.0f + v_dot_tr;
    float p = n_dot_br * v_dot_LVtr;
    float q = n_dot_LVtr * v_dot_LVtr;
    float s = v_dot_br * n_dot_LVtr;
    float x_num = q * ( -0.5f * p + 0.25f * v_dot_br * n_dot_LVtr );
    float x_denom = p * p + s * ( ( s - 2.0f * p ) )
        + n_dot_LVtr
            * ( ( n_dot_l * radius_cos + n_dot_v ) * v_dot_LVtr * v_dot_LVtr
                + q * ( -0.5f * ( v_dot_LVtr + v_dot_l * radius_cos ) - 0.5f ) );
    float twox1 = 2.0f * x_num / ( x_denom * x_denom + x_num * x_num );
    float sin_theta = twox1 * x_denom;
    float cos_theta = 1.0f - twox1 * x_num;

    n_dot_tr = cos_theta * n_dot_tr + sin_theta * n_dot_br;
    v_dot_tr = cos_theta * v_dot_tr + sin_theta * v_dot_br;

    // Calculate (N dot h)^2 based on bent light vector
    float new_n_dot_l = n_dot_l * radius_cos + n_dot_tr;
    float new_v_dot_l = v_dot_l * radius_cos + v_dot_tr;
    float n_dot_h = n_dot_v + new_n_dot_l;
    float h_dot_h = 2.0f * new_v_dot_l + 2.0f;
    return max( 0.0f, n_dot_h * n_dot_h / h_dot_h );
}

// Credit:
// https://github.com/sw-52/gluon/blob/d17988ea20a3ba092a308f01f147f120349d2489/shaders/include/light/specular_lighting.glsl#L20
float get_NoH_squared( float NoL, float NoV, float LoV, float light_radius )
{
    float radius_cos = cos( light_radius );
    float radius_tan = tan( light_radius );

    // Early out if R falls within the disc
    float RoL = 2.0 * NoL * NoV - LoV;
    if ( RoL >= radius_cos )
        return 1.0;

    float r_over_length_t = radius_cos * radius_tan * rsqrt( 1.0 - RoL * RoL );
    float not_r = r_over_length_t * ( NoV - RoL * NoL );
    float vot_r = r_over_length_t * ( 2.0 * NoV * NoV - 1.0 - RoL * LoV );

    // Calculate dot(cross(N, L), V). This could already be calculated and available.
    float triple
        = sqrt( saturate( 1.0 - NoL * NoL - NoV * NoV - LoV * LoV + 2.0 * NoL * NoV * LoV ) );

    // Do one Newton iteration to improve the bent light Direction
    float NoB_r = r_over_length_t * triple, VoB_r = r_over_length_t * ( 2.0 * triple * NoV );
    float NoL_vt_r = NoL * radius_cos + NoV + not_r, LoV_vt_r = LoV * radius_cos + 1.0 + vot_r;
    float p = NoB_r * LoV_vt_r, q = NoL_vt_r * LoV_vt_r, s = VoB_r * NoL_vt_r;
    float x_num = q * ( -0.5 * p + 0.25 * VoB_r * NoL_vt_r );
    float x_denom = p * p + s * ( ( s - 2.0 * p ) )
        + NoL_vt_r
            * ( ( NoL * radius_cos + NoV ) * LoV_vt_r * LoV_vt_r
                + q * ( -0.5 * ( LoV_vt_r + LoV * radius_cos ) - 0.5 ) );
    float two_x_1 = 2.0 * x_num / ( x_denom * x_denom + x_num * x_num );
    float sin_theta = two_x_1 * x_denom;
    float cos_theta = 1.0 - two_x_1 * x_num;
    not_r = cos_theta * not_r + sin_theta * NoB_r; // use new T to update not_r
    vot_r = cos_theta * vot_r + sin_theta * VoB_r; // use new T to update vot_r

    // Calculate (N.H)^2 based on the bent light direction
    float new_NoL = NoL * radius_cos + not_r;
    float new_LoV = LoV * radius_cos + vot_r;
    float NoH = NoV + new_NoL;
    float HoH = 2.0 * new_LoV + 2.0;

    return saturate( NoH * NoH / HoH );
}

float get_GGX_from_n_dot_h_sq( float n_dot_h_sq, float alpha )
{
    float alpha_squared = alpha * alpha;
    float partial_denom = n_dot_h_sq * ( alpha_squared - 1.0f ) + 1.0f;
    return alpha_squared / ( math.PI * partial_denom * partial_denom );
}

float3 evaluate_spherical_area_GGX(
    BSDF_Dots dots, float light_radius, float light_center_distance, float alpha, float3 F0 )
{
    float n_dot_h_sq
        = get_NoH_squared( dots.clamped_n_dot_l, dots.n_dot_v, dots.v_dot_l, light_radius );
    float density = get_GGX_from_n_dot_h_sq( n_dot_h_sq, alpha );
    float visibility = GeometrySmith( dots.n_dot_v, dots.n_dot_l, alpha )
        / ( 4.0f * dots.n_dot_v * dots.n_dot_l );
    float3 fresnel = FresnelSchlick( dots.h_dot_l, F0 );

    return fresnel * density * visibility;

    // float radius_tan = max( 0.001f, light_radius / light_center_distance - alpha * 0.25f );
    // float n_dot_h_sq = get_max_n_dot_h_sq( radius_tan, dots.n_dot_l, dots.n_dot_v, dots.v_dot_l
    // ); float density = get_GGX_from_n_dot_h_sq( n_dot_h_sq, alpha ); float visibility =
    // GeometrySmith( dots.n_dot_v, dots.n_dot_l, alpha )
    //     / ( 4.0f * dots.n_dot_v * dots.n_dot_l );
    // float3 fresnel = FresnelSchlick( dots.h_dot_l, F0 );
    // float3 brdf = density * visibility * fresnel;

    // // Normalization logic from Decima people
    // float alpha_squared_l_dot_h = alpha * alpha * ( dots.h_dot_l + 0.001f );
    // float normalization = alpha_squared_l_dot_h
    //     / ( alpha_squared_l_dot_h + 0.25f * light_radius * ( 3.0f * alpha + light_radius ) );

    // return normalization * brdf * saturate( dots.n_dot_l );
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

// 10 / ( 100 - (-1) * 99.9 )

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


