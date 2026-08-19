#include "../common.slang"
#include "../utils.slang"

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT0;
    float2 uv : TEXCOORD0;
    float2 id : IDS;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    float3 position;
    float4 prev_clip_pos;
    float4 curr_clip_pos;
    float3 normal;
    float4 tangent;
    float2 uv;
};

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_buffer_data;
layout( binding = 1, set = 0 ) ConstantBuffer<DebugData> debug_data;

layout( binding = 0, set = 1 ) Texture2D<float4> albedo_map;
layout( binding = 1, set = 1 ) Texture2D<float4> normal_map;
layout( binding = 2, set = 1 ) Texture2D<float4> metallic_roughness_map;
layout( binding = 3, set = 1 ) ConstantBuffer<MaterialData> material_data;

layout( binding = 0, set = 2 ) ConstantBuffer<ModelMatData> model_mat_data;

layout( binding = 0, set = 3 ) Texture2D<float2> BRDF_LUT;
layout( binding = 1, set = 3 ) Texture2D<float4> glint_noise;
layout( binding = 2, set = 3 ) Texture2D<float4> octahedral_sky;
layout( binding = 3, set = 3 ) Texture2D<float4> octahedral_sky_mips;

layout( binding = 0, set = 4 ) SamplerState nearest_sampler;

static const float MIN_GLASS_ROUGHNESS = 0.02f;

float3 sample_glossy_ibl( float roughness, float2 direction )
{
    float lod = roughness * SKY_MAX_MIP;
    uint lod0 = uint( floor( lod ) );
    uint lod1 = min( lod0 + 1, uint( SKY_MAX_MIP ) );
    float lod_t = lod - lod0;

    float3 glossy_lod0 = octahedral_sky_mips.SampleLevel( nearest_sampler, direction, lod0 ).rgb;
    float3 glossy_lod1 = octahedral_sky_mips.SampleLevel( nearest_sampler, direction, lod1 ).rgb;
    return lerp( glossy_lod0, glossy_lod1, lod_t );
}

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    VertexOutput output;
    float4 clip_pos = mul(
        mul( camera_buffer_data.mvp, model_mat_data.model_mat ), float4( input.position, 1.0 ) );

    float4 curr_clip_pos = mul(
        mul( camera_buffer_data.unjittered_mvp, model_mat_data.model_mat ),
        float4( input.position, 1.0f ) );

    float4 prev_clip_pos = mul( mul( camera_buffer_data.prev_mvp, model_mat_data.prev_model_mat ),
        float4( input.position, 1.0f ) );

    output.sv_position = clip_pos;
    output.curr_clip_pos = curr_clip_pos;
    output.prev_clip_pos = prev_clip_pos;

    output.position = mul(
        mul( camera_buffer_data.model, model_mat_data.model_mat ), float4( input.position, 1.0 ) )
                          .xyz;

    float3x3 normal_matrix
        = (float3x3)transpose( mul( model_mat_data.inv_model_mat, camera_buffer_data.inv_model ) );
    float3 normal = normalize( mul( normal_matrix, input.normal ) );
    float3 tangent = normalize( mul( normal_matrix, input.tangent.xyz ) );
    tangent = normalize( tangent - normal * dot( normal, tangent ) );

    output.normal = normal;
    output.tangent = float4( tangent, input.tangent.w );
    output.uv = input.uv;

    return output;
}

[shader( "fragment" )]
float4 fs_main( VertexOutput in ) : SV_Target
{
    float3 normal = normalize( in.normal );
    float3 view = normalize( camera_buffer_data.camera_pos.rgb - in.position );

    if ( material_data.has_normal_texture != 0 ) {
        float3 sampled_normal = normal_map.Sample( nearest_sampler, in.uv ).rgb;
        normal = normalize(
            lerp( normal,
                normalize( map_normals( sampled_normal, in.tangent, normal ) ),
                material_data.normal_texture_weight ) );
    }

    if ( dot( normal, view ) < 0.0f ) {
        normal = -normal;
    }

    float roughness = material_data.roughness;
    if ( material_data.has_metallic_roughness_texture != 0 ) {
        roughness = metallic_roughness_map.Sample( nearest_sampler, in.uv ).g;
    }
    roughness = clamp( roughness, MIN_GLASS_ROUGHNESS, 1.0f );

    float3 light = normalize( debug_data.sun_direction.rgb );
    float3 half = normalize( view + light );

    float n_dot_v = max( dot( normal, view ), 1e-4f );
    float n_dot_l = max( dot( normal, light ), 0.0f );
    float n_dot_h = max( dot( normal, half ), 0.0f );

    const float3 F0 = float3( 0.04f );

    float2 encoded_direction = oct_encode( reflect( -view, normal ) );
    float3 specular_indirect = sample_glossy_ibl( roughness, encoded_direction );

    float alpha = roughness * roughness;
    float NDF = DistributionGGX( n_dot_h, alpha );
    float G = GeometrySmith( n_dot_v, n_dot_l, alpha );
    float specular_direct = ( NDF * G * n_dot_l ) / ( 4.0f * n_dot_v * n_dot_l + 1e-4f );

    float3 reflected = specular_indirect + min( specular_direct, 55.0f );

    float fresnel = FresnelSchlickRoughness( n_dot_v, F0, roughness ).x;
    float tint_coverage = material_data.base_color.a;
    float opacity = saturate( fresnel + tint_coverage );

    float3 covered
        = reflected * fresnel + material_data.base_color.rgb * tint_coverage;
    float3 out_color = covered / max( opacity, 1e-3f );

    switch ( debug_data.debug_view ) {
    case DEBUG_VIEW_NORMALS:
        return float4( normal, 1.0f );
    case DEBUG_VIEW_ROUGHNESS_METAL:
        return float4( roughness, 0.0f, 0.0f, 1.0f );
    default:
        break;
    }

    return float4( out_color, opacity );
}
