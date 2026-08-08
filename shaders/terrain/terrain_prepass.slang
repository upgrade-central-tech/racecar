#include "../common.slang"
#include "../utils.slang"
#include "../glint/glint.hlsl"
#include "../clouds/cloud_utils.slang"

#include "terrain_common.slang"

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct VertexOutput {
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv;
};

// tesselation structs
struct HullOutput {
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv;
};

struct PatchConstantOutput {
    float edges[4] : SV_TessFactor;
    float inside[2] : SV_InsideTessFactor;
};

struct DomainOutput {
    float4 sv_position : SV_Position;
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv;
};

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_buffer_data;
layout( binding = 1, set = 0 ) ConstantBuffer<DebugData> debug_data;
layout( binding = 2, set = 0 ) ConstantBuffer<TerrainData> terrain_data;

layout( binding = 0, set = 1 ) Texture2D<float2> layer_test_mask;
// Specific material stuff
layout( binding = 1, set = 1 ) Texture2D<float4> grass_albedo_roughness;
layout( binding = 2, set = 1 ) Texture2D<float4> grass_normal_ao;

layout( binding = 3, set = 1 ) Texture2D<float4> asphalt_albedo_roughnes;
layout( binding = 4, set = 1 ) Texture2D<float4> asphalt_normal_ao;

layout( binding = 0, set = 2 ) SamplerState linear_sampler;

// LUT descs
layout( binding = 0, set = 3 ) Texture2D<float4> glint_noise;
layout( binding = 1, set = 3 ) Texture2D<float> terrain_noise;

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    VertexOutput output;

    output.position = mul( camera_buffer_data.model, float4( input.position, 1.0 ) ).xyz;

    float3x3 normal_matrix = (float3x3)transpose( camera_buffer_data.inv_model );
    float3 normal = normalize( mul( normal_matrix, input.normal ) );

    output.normal = normal;

    output.tangent = float4( 0.0, 0.0, 0.0, 0.0 );
    output.uv = float2( 0.0, 0.0 );

    return output;
}

float edge_tess_factor( float3 v0, float3 v1 )
{
    float3 mid = 0.5 * ( v0 + v1 );
    float dist = distance( mid, camera_buffer_data.camera_pos.xyz );
    float f = lerp( 32.0, 2.0, saturate( dist / 100.0 ) );
    return f;
}

[patchconstantfunc( "constants" )]
PatchConstantOutput constants( InputPatch<VertexOutput, 4> patch )
{
    PatchConstantOutput pc;

    pc.edges[0] = 1.0;
    pc.edges[1] = 1.0;
    pc.edges[2] = 1.0;
    pc.edges[3] = 1.0;
    pc.inside[0] = 1.0;
    pc.inside[1] = 1.0;

    pc.edges[0] = edge_tess_factor( patch[0].position, patch[2].position ); // top
    pc.edges[1] = edge_tess_factor( patch[0].position, patch[1].position ); // top
    pc.edges[2] = edge_tess_factor( patch[1].position, patch[3].position ); // top
    pc.edges[3] = edge_tess_factor( patch[3].position, patch[2].position ); // top

    float3 center
        = 0.25 * ( patch[0].position + patch[1].position + patch[2].position + patch[3].position );

    pc.inside[0] = edge_tess_factor( center, center );
    pc.inside[1] = edge_tess_factor( center, center );

    return pc;
}

[shader( "hull" )]
[domain( "quad" )]
[patchconstantfunc( "constants" )]
[partitioning( "integer" )]
[outputtopology( "triangle_ccw" )]
[outputcontrolpoints( 4 )]
HullOutput ts_control_main( InputPatch<VertexOutput, 4> patch, uint i: SV_OutputControlPointID )
{
    HullOutput output;

    output.position = patch[i].position;
    output.normal = patch[i].normal;
    output.tangent = patch[i].tangent;
    output.uv = patch[i].uv;

    return output;
}

struct FragmentOutput {
    float4 position : SV_Target0;
    float4 normal : SV_Target1;
    float4 albedo : SV_Target2;
    float4 packed_data : SV_Target3;
    float2 velocity : SV_Target4;
}

[shader( "domain" )]
[domain( "quad" )]
DomainOutput ts_eval_main(
    PatchConstantOutput pc, const OutputPatch<HullOutput, 4> patch, float2 uv: SV_DomainLocation )
{
    DomainOutput outVal;

    float3 p = patch[0].position * ( 1 - uv.x ) * ( 1 - uv.y )
        + patch[1].position * ( uv.x ) * ( 1 - uv.y ) + patch[3].position * ( uv.x ) * ( uv.y )
        + patch[2].position * ( 1 - uv.x ) * ( uv.y );

    float3 n = patch[0].normal * ( 1 - uv.x ) * ( 1 - uv.y )
        + patch[1].normal * ( uv.x ) * ( 1 - uv.y ) + patch[3].normal * ( uv.x ) * ( uv.y )
        + patch[2].normal * ( 1 - uv.x ) * ( uv.y );

    float4 tan = patch[0].tangent * ( 1 - uv.x ) * ( 1 - uv.y )
        + patch[1].tangent * ( uv.x ) * ( 1 - uv.y ) + patch[3].tangent * ( uv.x ) * ( uv.y )
        + patch[2].tangent * ( 1 - uv.x ) * ( uv.y );

    float2 t = patch[0].uv * ( 1 - uv.x ) * ( 1 - uv.y ) + patch[1].uv * ( uv.x ) * ( 1 - uv.y )
        + patch[3].uv * ( uv.x ) * ( uv.y ) + patch[2].uv * ( 1 - uv.x ) * ( uv.y );

    float2 uv_scrolling_offset = terrain_data.terrain_data1.xy;
    p = terrain_displace( p, uv_scrolling_offset, layer_test_mask, linear_sampler );

    outVal.sv_position = mul( camera_buffer_data.mvp, float4( p, 1.0 ) );
    outVal.position = p;
    outVal.normal = n;
    outVal.tangent = tan;
    outVal.uv = t;

    return outVal;
}

/// Gathers the globally-bound terrain textures into the bundle terrain_common.slang wants.
TerrainMaps get_terrain_maps()
{
    TerrainMaps maps;
    maps.layer_mask = layer_test_mask;
    maps.grass_albedo_roughness = grass_albedo_roughness;
    maps.grass_normal_ao = grass_normal_ao;
    maps.asphalt_albedo_roughness = asphalt_albedo_roughnes;
    maps.asphalt_normal_ao = asphalt_normal_ao;
    maps.linear_sampler = linear_sampler;
    return maps;
}

[shader( "fragment" )]
FragmentOutput fs_main( DomainOutput in )
{
    FragmentOutput output;

    // Stencil is defined as such:
    // 0 - nothing shaded
    // 1 - car shading
    // 2 - terrain shading
    float stencil = TERRAIN_ID;

    output.position = float4( in.position, stencil );

    float3 world_position = in.position;
    float3 normal = in.normal;

    //=======================================================================================
    // GLOBAL MAP UV EVALUATIONS
    //=======================================================================================
    float2 uv_scrolling_offset = terrain_data.terrain_data1.xy;

    TerrainUVs uvs = terrain_compute_uvs( world_position, uv_scrolling_offset );
    float2 world_uv_unwrapped = uvs.world_uv_unwrapped;

    float snowness = terrain_data.terrain_data0.z; // terrain_data.snow;
    float wetness_toggle = terrain_data.terrain_data0.y; // terrain_data.wetness;

    //=======================================================================================
    // Material gather. Shared with the ray tracing path via terrain_common.slang; being a
    // fragment shader we have real derivatives, so hand those over rather than taking the
    // mip 0 fallback.
    //=======================================================================================
    TerrainGradients gradients;
    gradients.layer_ddx = ddx( uvs.layer_uv );
    gradients.layer_ddy = ddy( uvs.layer_uv );
    gradients.material_ddx = ddx( uvs.world_uv );
    gradients.material_ddy = ddy( uvs.world_uv );

    TerrainSurface surface = terrain_evaluate_surface( world_position, normal,
        uv_scrolling_offset, uvs, snowness, wetness_toggle, get_terrain_maps(), gradients );

    float3 mixed_albedo = surface.albedo;
    float3 mixed_normal = surface.normal;
    float mixed_roughness = surface.roughness;
    float mixed_ao = surface.ao;
    float wetness = surface.wetness;
    float snow_lerp = surface.snow_lerp;

    float3 light = normalize( debug_data.sun_direction.rgb );
    float3 view = normalize( camera_buffer_data.camera_pos.rgb - world_position );
    float3 half_vector = normalize( view + light );
    BSDF_Dots dots = get_dots( mixed_normal, view, half_vector, light );

    float n_dot_h_sq = get_NoH_squared( dots.n_dot_l, dots.n_dot_v, dots.v_dot_l, 0.035f );
    float density = get_GGX_from_n_dot_h_sq( n_dot_h_sq, mixed_roughness * mixed_roughness );

    if ( snow_lerp > 0.2f ) {
        float2 duvdx = ddx( world_uv_unwrapped );
        float2 duvdy = ddy( world_uv_unwrapped );
        float3 local_half = world_to_local( half_vector, normal );

        const float glint_log_density = 24.0f;
        const float glint_roughness = 0.20f;
        const float glint_randomness = 3.0f;

        float maxNDF = 10000.0f;
        float targetNDF = density;
        density = lerp( density,
            2.0f
                * SampleGlints2023NDF( local_half, targetNDF, maxNDF, world_uv_unwrapped, duvdx,
                    duvdy, glint_log_density, glint_roughness, glint_randomness )
                    .x,
            snow_lerp );
    }

    terrain_apply_wetness_exposure( wetness_toggle, mixed_albedo, mixed_roughness );

    output.packed_data = float4( wetness, min( density, 65.f ), 0.0f, 0.0f );

    output.normal = float4( mixed_normal, mixed_ao );
    output.albedo = float4( mixed_albedo, mixed_roughness );


    output.velocity = float2( 0.0f );

    return output;
}


