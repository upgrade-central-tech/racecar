#include "../common.slang"

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
    float2 uv;
};

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_buffer_data;
layout( binding = 1, set = 0 ) ConstantBuffer<DebugData> debug_data;

// Material stuff strictly
layout( binding = 0, set = 1 ) Texture2D<float4> albedo_map;
layout( binding = 1, set = 1 ) Texture2D<float4> normal_map;
layout( binding = 2, set = 1 ) Texture2D<float4> metallic_roughness_map;
layout( binding = 3, set = 1 ) ConstantBuffer<MaterialData> material_data;

// Cubemaps + LUTs
layout( binding = 0, set = 2 ) Texture2D<float2> BRDF_LUT;
layout( binding = 1, set = 2 ) Texture2D<float4> glint_noise;
layout( binding = 2, set = 2 ) Texture2D<float4> octahedral_sky;
layout( binding = 3, set = 2 ) Texture2D<float4> octahedral_sky_irradiance;
layout( binding = 4, set = 2 ) Texture2D<float4> octahedral_sky_mips;

layout( binding = 0, set = 3 ) SamplerState nearest_sampler;

static const float PI = 3.14159265359;

#include "../glint/glint.hlsl"

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

float DistributionGGX( float n_dot_h, float alpha )
{
    float alpha2 = max( alpha * alpha, 0.001f );

    float n_dot_h_2 = n_dot_h * n_dot_h;
    float denom = ( n_dot_h_2 ) * ( alpha2 - 1.0f ) + 1.0f;

    return alpha2 / ( PI * denom * denom );
}

float3 world_to_local( float3 world_dir, float3 normal )
{
    float3 up = abs( normal.z ) < 0.999f ? float3( 0, 0, 1 ) : float3( 1, 0, 0 );
    float3 tangent = normalize( cross( up, normal ) );
    float3 bitangent = cross( normal, tangent );
    float3x3 TBN = float3x3( tangent, bitangent, normal );

    return mul( transpose( TBN ), world_dir );
}

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    VertexOutput output;
    output.sv_position = mul( camera_buffer_data.mvp, float4( input.position, 1.0 ) );
    output.position = mul( camera_buffer_data.model, float4( input.position, 1.0 ) ).xyz;

    float3x3 normal_matrix = (float3x3)transpose( camera_buffer_data.inv_model );
    float3 normal = normalize( mul( normal_matrix, input.normal ) );
    float3 tangent = normalize( mul( normal_matrix, input.tangent.xyz ) );
    tangent = normalize( tangent - normal * dot( normal, tangent ) );

    output.normal = normal;
    output.tangent = float4( tangent, input.tangent.w );
    output.uv = input.uv;

    return output;
}

struct FragmentOutput {
    float4 position : SV_Target0;
    float4 normal : SV_Target1;
    float4 tangent : SV_Target2;
    float4 uv : SV_Target3;
    float4 albedo : SV_Target4;
    /// roughness, metallic, clearcoat roughness, clearcoat weight
    float4 packed_data : SV_Target5;
}

[shader( "fragment" )]
FragmentOutput fs_main( VertexOutput in )
{
    FragmentOutput output;

    // Stencil is defined as such:
    // 0 - nothing shaded
    // 1 - car shading
    // 2 - terrain shading
    float stencil = CAR_ID;

    if ( material_data.has_base_color_texture != 0 && debug_data.enable_albedo_map != 1 ) {
        output.albedo = albedo_map.Sample( nearest_sampler, in.uv );
    } else {
        output.albedo = material_data.base_color;
    }

    if ( material_data.has_metallic_roughness_texture != 0
        && debug_data.enable_roughness_metal_map ) {
        output.packed_data = float4( metallic_roughness_map.Sample( nearest_sampler, in.uv ).gb,
            material_data.clearcoat_roughness, material_data.clearcoat );
    } else {
        output.packed_data = float4( material_data.roughness, material_data.metallic,
            material_data.clearcoat_roughness, material_data.clearcoat );
    }

#if 1
    float3 normal = normalize( in.normal );
    float3 light = normalize( debug_data.sun_direction.rgb );
    float3 view = normalize( camera_buffer_data.camera_pos.rgb - in.position );
    float3 half = normalize( view + light );
    float3 local_half = world_to_local( half, normal );

    float2 duvdx = ddx( in.uv );
    float2 duvdy = ddy( in.uv );

    float alpha = material_data.roughness * material_data.roughness;
    float clamped_n_dot_h = max( dot( normal, half ), 0.0f );
    float NDF = DistributionGGX( clamped_n_dot_h, alpha );

    float maxNDF = 10000.0f;
    float targetNDF = NDF;
    NDF = SampleGlints2023NDF( local_half, targetNDF, maxNDF, in.uv, duvdx, duvdy ).x;
#endif

    output.position = float4( in.position, stencil );
    output.normal = float4( in.normal, 1.0f );
    if ( debug_data.enable_normal_map && material_data.has_normal_texture != 0 ) {
        float3 normal_map_norm = normal_map.Sample( nearest_sampler, in.uv ).rgb;
        float3 new_normal = map_normals( normal_map_norm, in.tangent, in.normal );
        if ( !any( isnan( new_normal ) ) ) {
            output.normal = float4( new_normal, 1.0f );
        }
    }

    output.tangent = in.tangent;
    output.uv = float4( in.uv, NDF, 0.0f );

    return output;
}


