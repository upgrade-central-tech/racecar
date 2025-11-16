struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT0;
    float2 uv : TEXCOORD0;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    float3 position;
    float3 normal;
    float4 tangent;
    float3 color;
    float2 uv;
};

struct CameraBufferData {
    float4x4 mvp;
    float4x4 model;
    float4x4 inv_model;
    float3 camera_pos;
    float3 color;
};

struct DebugData {
    float4 color;

    // Pack: roughness, metallic, clearcoat roughness, and clearcoat weight respsectively
    float4 packed_data0;

    bool enable_albedo_map;
    bool enable_normal_map;
    bool enable_roughness_metal_map;
    bool normals_only;
    bool albedo_only;
    bool roughness_metal_only;
}

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_buffer_data;
layout( binding = 1, set = 0 ) ConstantBuffer<DebugData> debug_data;

// Material stuff strictly
layout( binding = 0, set = 1 ) Texture2D<float4> albedo_map;
layout( binding = 1, set = 1 ) Texture2D<float4> normal_map;
layout( binding = 2, set = 1 ) Texture2D<float4> metallic_roughness_map;

// Cubemaps + LUTs
layout( binding = 0, set = 2 ) TextureCube<float4> cubemap;
layout( binding = 1, set = 2 ) Texture2D<float2> BRDF_LUT;

layout( binding = 0, set = 3 ) SamplerState nearest_sampler;

static const float PI = 3.14159265359;

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    VertexOutput output;
    output.sv_position = mul( camera_buffer_data.mvp, float4( input.position, 1.0 ) );
    output.position = mul( camera_buffer_data.model, float4( input.position, 1.0 ) ).xyz;
    output.color = input.position.xyz;

    float3x3 normal_matrix = (float3x3)transpose( camera_buffer_data.inv_model );
    float3 normal = normalize( mul( normal_matrix, input.normal ) );
    float3 tangent = normalize( mul( normal_matrix, input.tangent.xyz ) );
    tangent = normalize( tangent - normal * dot( normal, tangent ) );

    output.normal = normal;
    output.tangent = float4( tangent, input.tangent.w );
    output.uv = input.uv;

    return output;
}

float DistributionGGX( float n_dot_h, float alpha )
{
    float alpha2 = max( alpha * alpha, 0.001f );

    float n_dot_h_2 = n_dot_h * n_dot_h;
    float denom = ( n_dot_h_2 ) * ( alpha2 - 1.0f ) + 1.0f;

    return alpha2 / ( PI * denom * denom );
}

float GeometrySchlick( float theta, float k ) { return theta / ( theta * ( 1.0f - k ) + k ); }

float GeometrySmith( float n_dot_v, float n_dot_l, float alpha )
{
    float num = ( alpha + 1.0f );
    float k = ( num * num ) / 8.0f;

    return GeometrySchlick( n_dot_v, k ) * GeometrySchlick( n_dot_l, k );
}

float3 FresnelSchlick( float cos_theta, float3 F0 )
{
    return F0 + ( 1.0f - F0 ) * pow( 1.0f - cos_theta, 5.0f );
}

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

float DistributionGTR( float alpha, float h_dot_l )
{
    float alpha_2 = alpha * alpha;
    return ( alpha_2 - 1.0f )
        / ( PI * log( alpha_2 ) * ( 1.0f + ( alpha_2 - 1.0f ) * ( h_dot_l * h_dot_l ) ) );
}

[shader( "fragment" )]
float4 fs_main( VertexOutput in )
    : SV_Target
{
    // Texture samples
    float3 base_albedo = debug_data.enable_albedo_map
        ? albedo_map.Sample( nearest_sampler, in.uv ).rgb
        : debug_data.color.rgb;

    float3 normal_map_norm = normal_map.Sample( nearest_sampler, in.uv ).rgb;

    float2 metallic_roughness = debug_data.enable_roughness_metal_map
        ? metallic_roughness_map.Sample( nearest_sampler, in.uv ).gb
        : float2( debug_data.packed_data0.x, debug_data.packed_data0.y );

    // Material properties
    float metallic = metallic_roughness.y;
    float roughness = metallic_roughness.x;

    // Basic vectors
    float3 light_pos = float3( 1, 2, 1 );
    float3 light = normalize( light_pos - in.position );
    float3 radiance = float3( 1, 1, 1 );
    float attenuation = 1.0f / length( light_pos - in.position );
    attenuation *= attenuation;

    float3 camera_pos = camera_buffer_data.camera_pos;
    float3 normal = debug_data.enable_normal_map
        ? map_normals( normal_map_norm, in.tangent, in.normal )
        : normalize( in.normal );
    float3 view = normalize( camera_pos - in.position );
    float3 half = normalize( view + light );

    float3 F0 = float3( 0.04f );
    F0 = lerp( F0, base_albedo, metallic );

    // Essential dot products
    float n_dot_v = ( dot( normal, view ) );
    float n_dot_h = ( dot( normal, half ) );
    float n_dot_l = ( dot( normal, light ) );
    float h_dot_l = ( dot( half, light ) );
    float v_dot_h = ( dot( view, half ) );

    float clamped_n_dot_v = max( n_dot_v, 0.0f );
    float clamped_n_dot_h = max( n_dot_h, 0.0f );
    float clamped_n_dot_l = max( n_dot_l, 0.0f );
    float clamped_h_dot_l = max( h_dot_l, 0.0f );
    float clamped_v_dot_h = max( v_dot_h, 0.0f );

    ////////////////////////////////////////////////////////////////
    // Specular reflection
    ////////////////////////////////////////////////////////////////

    // Base specular
    float alpha = roughness * roughness;
    float NDF = DistributionGGX( clamped_n_dot_h, alpha );
    float G = GeometrySmith( clamped_n_dot_v, clamped_n_dot_l, alpha );
    float3 F = FresnelSchlick( clamped_n_dot_h, F0 );

    float2 brdf_uv = float2( clamped_n_dot_v, roughness );
    float2 brdf = BRDF_LUT.Sample( nearest_sampler, brdf_uv ).rg;

    // So far only the first mip exists. Will need some more complicated efforts to
    // get this stuff to work with roughness.
    float3 precomputed_lo_specular = cubemap.Sample( nearest_sampler, reflect( -view, normal ) ).rgb;

    // Point-light solution is found via:
    // float3 base_specular = ( NDF * G * F ) / ( 4.0f * clamped_n_dot_v * clamped_n_dot_l + 0.001f );
    // We use Split-sum approximation instead:
    float3 base_specular = precomputed_lo_specular * ( F0 * brdf.x + brdf.y );

    // Clearcoat evaluation
    float clearcoat = debug_data.packed_data0.w;
    float cc_roughness = debug_data.packed_data0.z;
    float alpha_cc = cc_roughness * cc_roughness;
    float D_cc = DistributionGTR( lerp( 0.1f, 0.001f, alpha_cc ), abs( n_dot_h ) );
    float G_cc = GeometrySmith( clamped_n_dot_v, clamped_n_dot_l, alpha_cc );
    float3 F_cc = FresnelSchlick( clamped_v_dot_h, float3( 0.04 ) );
    float3 specular_cc
        = ( F_cc * D_cc * G_cc ) / ( 4.0f * clamped_n_dot_v * clamped_n_dot_l + 0.001f );

    // brdf_cc needs to be prefiltered using the GTR1 (Generalized Trowbridge Reitz) NDF
    float3 F0_cc = float3( 0.04f );
    float2 brdf_cc = brdf;
    float3 precomputed_specular_cc = precomputed_lo_specular * ( F0_cc * brdf.x + brdf.y );

    float3 clearcoat_specular = clearcoat * 1.0f * precomputed_specular_cc;
    float3 specular_final = max( clearcoat_specular + ( 1.0f - F_cc ) * base_specular, 0.0f );

    ////////////////////////////////////////////////////////////////
    // Diffuse reflection
    // Based on Disney 2012 diffuse model:
    // https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf
    ////////////////////////////////////////////////////////////////

    float3 lambert = base_albedo / PI;

    // Fresnel weights
    float F_l = pow( 1.0f - clamped_n_dot_l, 5.0f );
    float F_v = pow( 1.0f - clamped_n_dot_v, 5.0f );
    float F_D90 = 0.5f + 2.0f * roughness * clamped_h_dot_l * clamped_h_dot_l;

    float3 diffuse_final
        = lambert * ( 1.0f + ( F_D90 - 1.0f ) * F_l ) * ( 1.0f + ( F_D90 - 1.0f ) * F_v );
    // float3 diffuse_final = lambert;

    ////////////////////////////////////////////////////////////////
    // Composite results
    ////////////////////////////////////////////////////////////////
    float3 base_color = diffuse_final + specular_final;
    float3 out_color = base_color * clamped_n_dot_l * ( radiance * 1.5f );

    return float4( out_color, 1.0f );
}


