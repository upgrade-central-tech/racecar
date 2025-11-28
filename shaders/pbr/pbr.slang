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
    bool enable_albedo_map;
    bool enable_normal_map;
    bool enable_roughness_metal_map;
    bool normals_only;
    bool albedo_only;
    bool roughness_metal_only;
}

struct MaterialData {
    int has_base_color_texture;
    int has_metallic_roughness_texture;
    int has_normal_texture;
    int has_emmisive_texture;

    float4 base_color;

    float metallic;
    float roughness;
    float normal_texture_weight;
    float ior;

    float3 specular_tint;
    float specular;

    float3 sheen_tint;
    float sheen_roughness;

    float sheen_weight;
    float transmission;
    float clearcoat;
    float clearcoat_roughness;

    float3 emissive;
    // 0 if lit, 1 if unlit
    int unlit = false;
}

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_buffer_data;
layout( binding = 1, set = 0 ) ConstantBuffer<DebugData> debug_data;

layout( binding = 0, set = 1 ) Texture2D<float4> albedo_map;
layout( binding = 1, set = 1 ) Texture2D<float4> normal_map;
layout( binding = 2, set = 1 ) Texture2D<float4> metallic_roughness_map;
layout( binding = 3, set = 1 ) ConstantBuffer<MaterialData> material_data;

layout( binding = 0, set = 2 ) SamplerState nearest_sampler;

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

float DistributionGGX( float n_dot_h, float roughness )
{
    float alpha = roughness * roughness;
    float alpha2 = max( alpha * alpha, 0.001f );

    float n_dot_h_2 = n_dot_h * n_dot_h;
    float denom = ( n_dot_h_2 ) * ( alpha2 - 1.0f ) + 1.0f;

    return alpha2 / ( denom * denom );
}

float GeometrySchlick( float theta, float k ) { return theta / ( theta * ( 1.0f - k ) + k ); }

float GeometrySmith( float n_dot_v, float n_dot_l, float roughness )
{
    float alpha = roughness * roughness;
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

[shader( "fragment" )]
float4 fs_main( VertexOutput in )
    : SV_Target
{
    // Texture samples
    float3 base_albedo = debug_data.enable_albedo_map
        ? albedo_map.Sample( nearest_sampler, in.uv ).rgb
        : camera_buffer_data.color;
    float3 normal_map_norm = normal_map.Sample( nearest_sampler, in.uv ).rgb;
    float2 metallic_roughness = debug_data.enable_roughness_metal_map
        ? metallic_roughness_map.Sample( nearest_sampler, in.uv ).gb
        : float2( 0.01f, 0.0f );

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
    float n_dot_v = max( dot( normal, view ), 0.0f );
    float n_dot_h = max( dot( normal, half ), 0.0f );
    float n_dot_l = max( dot( normal, light ), 0.0f );

    // Diffuse reflection
    float3 diffuse_albedo = base_albedo;
    float3 diffuse = diffuse_albedo;

    // Specular reflection
    float NDF = DistributionGGX( n_dot_h, roughness );
    float G = GeometrySmith( n_dot_v, n_dot_l, roughness );
    float3 F = FresnelSchlick( n_dot_h, F0 );

    float3 specular = ( NDF * F * G ) / ( 4.0f * n_dot_v * n_dot_l );

    float3 out_color = ( 1.0f - F ) * diffuse + specular;
    out_color *= n_dot_l * attenuation * radiance;

    // Gamma
    // out_color = out_color / (out_color + float3(1.0f));
    // out_color = pow(out_color, float3(1.0f / 2.2f));

    out_color = debug_data.normals_only ? normal : out_color;
    out_color = debug_data.roughness_metal_only ? float3( roughness, metallic, 0.0f ) : out_color;
    out_color = debug_data.albedo_only ? base_albedo : out_color;

    return float4( out_color, 1.0 );
}


