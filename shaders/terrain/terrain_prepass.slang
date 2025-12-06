#include "../common.slang"
#include "../utils.slang"
#include "../clouds/cloud_utils.slang"

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

layout( binding = 0, set = 1 ) Texture2D<float2> layer_test_mask;
// Specific material stuff
layout( binding = 1, set = 1 ) Texture2D<float4> grass_albedo_roughness;
layout( binding = 2, set = 1 ) Texture2D<float4> grass_normal_ao;

layout( binding = 3, set = 1 ) Texture2D<float4> asphalt_albedo_roughnes;
layout( binding = 4, set = 1 ) Texture2D<float4> asphalt_normal_ao;

layout( binding = 0, set = 2 ) SamplerState linear_sampler;

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
    float f = lerp( 16.0, 2.0, saturate( dist / 60.0 ) );
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
[outputtopology( "triangleccw" )]
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

    outVal.sv_position = mul( camera_buffer_data.mvp, float4( p, 1.0 ) );
    outVal.position = p;
    outVal.normal = n;
    outVal.tangent = tan;
    outVal.uv = t;

    return outVal;
}

void evaluate_wetness( float wetness, float porosity, inout float3 color, inout float roughness,
    inout float ao, inout float3 world_normal )
{
    // https://advances.realtimerendering.com/other/2016/naughty_dog/NaughtyDog_TechArt_Final.pdf
    float3 color_sq = color * color;
    color = lerp( color, color_sq, clamp_range( wetness, 0.0f, 0.35f ) * porosity );
    roughness = lerp( roughness, 0.1f, clamp_range( wetness, 0.2f, 1.0f ) );
    ao = lerp( ao, 1.0f, clamp_range( wetness, 0.45f, 0.95f ) );

    // GBuffer doesn't store the vertex normal, so we'll treat 0.45 case and 0.98 case identically.
    // Uncharted 4, however, uses frag normal for 0.45 and vert for 0.98.
    world_normal = normalize(
        lerp( world_normal, float3( 0.0f, 1.0f, 0.0f ), clamp_range( wetness, 0.45f, 1.0f ) ) );
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

    // UV Distortion to reduce layer-mask "blockiness" from texture sampling
    float distort_scale = 5.0f;
    float rand_sx = perlin_noise3D( distort_scale * world_position );
    float rand_sy = rand_sx; // perlin_noise3D( distort_scale * world_position );
    float distort_amplitude = 0.1f;
    float2 distortion_offset
        = 2.0f * float2( rand_sx, rand_sy ) * distort_amplitude - distort_amplitude;

    // Layer map texture
    float layer_map_scale = 0.025f;
    float world_scale = 0.05f;

    float2 uv = frac( ( distortion_offset + float2( world_position.x - 18.0f, world_position.z ) )
        * layer_map_scale );
    float2 world_uv_unwrapped = world_position.xz * world_scale;
    float2 world_uv = world_uv_unwrapped - floor( world_uv_unwrapped );

    // = frac( float2( world_position.x, world_position.z ) * world_scale );
    // uv = select( uv < 0.0f, uv + 1.0f, uv );

    float2 duvdx = ddx( world_uv_unwrapped );
    float2 duvdy = ddy( world_uv_unwrapped );

    // Naive-sample
    // So far:
    // R - Asphalt
    // G - Grass/Terrain mask
    float2 layers = layer_test_mask.Sample( linear_sampler, uv );

    //=======================================================================================
    // Material property gather. TODO: use a struct?
    //=======================================================================================
    float4 grass_color_roughness = grass_albedo_roughness.Sample( linear_sampler, world_uv );
    float4 grass_normal_map_ao = grass_normal_ao.Sample( linear_sampler, world_uv );
    float3 grass_normal_map = grass_normal_map_ao.rgb;
    float3 grass_color = grass_color_roughness.rgb;
    float3 grass_normal
        = normalize( map_normals( grass_normal_map, float4( 0, 0, 1, -1 ), normal ) );
    float grass_ao = grass_normal_map_ao.a;

    float4 asphalt_color_roughness = asphalt_albedo_roughnes.Sample( linear_sampler, world_uv );
    float4 asphalt_normal_map_ao = asphalt_normal_ao.Sample( linear_sampler, world_uv );
    float3 asphalt_normal_map = asphalt_normal_map_ao.rgb;
    float3 asphalt_color = asphalt_color_roughness.rgb;
    float3 asphalt_normal
        = normalize( map_normals( asphalt_normal_map, float4( 1, 0, 0, -1 ), normal ) );
    float asphalt_ao = asphalt_normal_map_ao.a;

    // CORRECTION: The current texture stores "glossiness". Need to minus1 to convert to roughness.
    float asphalt_roughness = 1.0f - asphalt_color_roughness.a;
    float grass_roughness = 1.0f - grass_color_roughness.a;

    //=======================================================================================
    // Final material gather. These params get passed into the respective lighting
    // functions below.
    //=======================================================================================

    // Naive lerping??? What's a better solution?
    float3 mixed_albedo = lerp( float3( 1.0f, 0.0f, 0.0f ), asphalt_color, layers.x );
    mixed_albedo = lerp( mixed_albedo, grass_color, layers.y );

    // This is so obviously wrong, but we're going to try this out for now.
    float3 mixed_normal = lerp( float3( 0.0f, 1.0f, 0.0f ), asphalt_normal, layers.x );
    mixed_normal = lerp( mixed_normal, grass_normal, layers.y );

    // This is also so wrong.
    float mixed_ao = lerp( 1.0f, asphalt_ao, layers.x );
    mixed_ao = lerp( mixed_ao, grass_ao, layers.y );

    // ?? You know the drill
    float mixed_roughness = lerp( 1.0f, asphalt_roughness, layers.x );
    mixed_roughness = lerp( mixed_roughness, grass_roughness, layers.y );

    // Distribute the wetness around da world...
    float puddle_scale = 0.5f;
    float small_puddle_map = perlin_noise3D( puddle_scale * world_position ) + 0.25f;
    float big_puddle_map = perlin_noise3D( 0.125f * puddle_scale * world_position ) + 0.50f;

    float wetness = clamp_range( big_puddle_map, 0.35f, 0.75f )
        + clamp_range( small_puddle_map, 0.1f, 0.95f );

    // THE WORLD IS ALL WET.
    float wetness_toggle = 1.0f;
    float mixed_wetness = lerp( 0.0f, wetness, wetness_toggle );
    float mixed_porosity = 0.0f;
    evaluate_wetness(
        mixed_wetness, mixed_porosity, mixed_albedo, mixed_roughness, mixed_ao, mixed_normal );

    float3 light = normalize( debug_data.sun_direction.rgb );
    float3 view = normalize( camera_buffer_data.camera_pos.rgb - world_position );
    float3 half_vector = normalize( view + light );
    BSDF_Dots dots = get_dots( mixed_normal, view, half_vector, light );

    float n_dot_h_sq = get_NoH_squared( dots.n_dot_l, dots.n_dot_v, dots.v_dot_l, 0.035f );
    float density = get_GGX_from_n_dot_h_sq( n_dot_h_sq, mixed_roughness * mixed_roughness );

    output.normal = float4( mixed_normal, mixed_ao );
    output.albedo = float4( mixed_albedo, mixed_roughness );
    output.packed_data = float4( wetness, density, 0.0f, 0.0f );

    return output;
}


