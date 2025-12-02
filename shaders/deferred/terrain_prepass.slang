#include "../common.slang"
#include "../utils.slang"
#include "../clouds/cloud_utils.slang"

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv;
};

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_buffer_data;

layout( binding = 0, set = 1 ) Texture2D<float2> layer_test_mask;
// Specific material stuff
layout( binding = 1, set = 1 ) Texture2D<float4> grass_albedo_roughness;
layout( binding = 2, set = 1 ) Texture2D<float4> grass_normal_ao;

layout( binding = 0, set = 2 ) SamplerState linear_sampler;

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    VertexOutput output;
    output.sv_position = mul( camera_buffer_data.mvp, float4( input.position, 1.0 ) );
    output.position = mul( camera_buffer_data.model, float4( input.position, 1.0 ) ).xyz;

    float3x3 normal_matrix = (float3x3)transpose( camera_buffer_data.inv_model );
    float3 normal = normalize( mul( normal_matrix, input.normal ) );

    output.normal = normal;

    return output;
}

struct FragmentOutput {
    float4 position : SV_Target0;
    float4 normal : SV_Target1;
    float4 albedo : SV_Target2;
}

[shader( "fragment" )]
FragmentOutput fs_main( VertexOutput in )
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
    // R - Snow
    // G - Grass
    // B - Asphalt?
    float2 layers = layer_test_mask.Sample( linear_sampler, uv );

    //=======================================================================================
    // Material property gather. TODO: use a struct?
    //=======================================================================================
    float4 grass_color_roughness = grass_albedo_roughness.Sample( linear_sampler, world_uv );
    float4 grass_normal_map_ao = grass_normal_ao.Sample( linear_sampler, world_uv );
    float3 grass_normal_map = grass_normal_map_ao.rgb;
    float3 grass_normal = normalize(map_normals( grass_normal_map, float4( 0, 0, 1, -1 ), normal ));
    float3 grass_color = grass_color_roughness.rgb;

    // Bias towards the top.
    grass_normal = normalize( grass_normal + float3( 0.0f, 0.1f, 0.0f ) );

    float grass_roughness = grass_color_roughness.a;
    float grass_ao = grass_normal_map_ao.a;

    float3 snow_color = float3( 1.0f, 1.0f, 1.0f );
    float3 snow_normal = float3( 0.0f, 1.0f, 0.0f );
    float snow_roughness = 0.1f;
    float snow_ao = 1.0f;

    //=======================================================================================
    // Final material gather. These params get passed into the respective lighting
    // functions below.
    //=======================================================================================

    // Naive lerping??? What's a better solution?
    float3 mixed_albedo = lerp( float3( 0.0f ), snow_color, layers.x );
    mixed_albedo = lerp( mixed_albedo, grass_color, layers.y );

    // This is so obviously wrong, but we're going to try this out for now.
    float3 mixed_normal = lerp( float3( 0.0f, 1.0f, 0.0f ), snow_normal, layers.x );
    mixed_normal = lerp( mixed_normal, grass_normal, layers.y );

    // This is also so wrong.
    float mixed_ao = lerp( 1.0f, snow_ao, layers.x );
    mixed_ao = lerp( mixed_ao, grass_ao, layers.y );

    // ?? You know the drill
    float mixed_roughness = lerp( 1.0f, snow_roughness, layers.x );
    mixed_roughness = lerp( mixed_roughness, grass_roughness, layers.y );

    output.normal = float4( mixed_normal, mixed_ao );
    output.albedo = float4( mixed_albedo, mixed_roughness );

    return output;
}


