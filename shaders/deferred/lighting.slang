
struct VertexInput {
    float2 position : POSITION;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    float2 uv;
};

struct CameraBufferData {
    float4x4 mvp;
    float4x4 model;
    float4x4 inv_model;
    float4 camera_pos;
    // Pack: near plane, far plane, aspect ratio, fov_Y
    float4 camera_constants;
};

struct DebugData {
    float4 color;

    // Pack: roughness, metallic, clearcoat roughness, and clearcoat weight respsectively
    float4 packed_data0;
    float4 sun_direction;

    bool enable_albedo_map;
    bool enable_normal_map;
    bool enable_roughness_metal_map;
    bool normals_only;
    bool albedo_only;
    bool roughness_metal_only;
}

struct SHData {
    float4 coeff0;
    float4 coeff1;
    float4 coeff2;
    float4 coeff3;
    float4 coeff4;
    float4 coeff5;
    float4 coeff6;
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
layout( binding = 2, set = 2 ) TextureCube<float4> irradiance_cubemap;
layout( binding = 3, set = 2 ) Texture2D<float4> irradiance_SH;
layout( binding = 4, set = 2 ) Texture2D<float4> glint_noise;
layout( binding = 5, set = 2 ) Texture2D<float4> octahedral_sky;
layout( binding = 6, set = 2 ) ConstantBuffer<SHData> sh_data;

layout( binding = 0, set = 3 ) SamplerState nearest_sampler;
layout( binding = 1, set = 3 ) SamplerState point_sampler;

layout( binding = 0, set = 4 ) Texture2D<float4> GBuffer_Position;
layout( binding = 1, set = 4 ) Texture2D<float4> GBuffer_Normals;
layout( binding = 2, set = 4 ) Texture2D<float4> GBuffer_Tangent;
layout( binding = 3, set = 4 ) Texture2D<float4> GBuffer_UV;
layout( binding = 4, set = 4 ) Texture2D<float4> GBuffer_Albedo;
layout( binding = 5, set = 4 ) Texture2D<float> GBuffer_Depth;
layout( binding = 6, set = 4 ) Texture2DMS<float> GBuffer_DepthMS;
// roughness, metallic, clearcoat roughness, clearcoat weight
layout( binding = 7, set = 4 ) Texture2D<float4> GBuffer_Packed_Data;

// #include "../utils.slang"
// #include "../glint/glint_noise_init.hlsl"
// #include "../glint/glint.hlsl"

#include "../car_mat/car_lighting.slang"

float LinearDepth( float perspective_depth, float near, float far )
{
    return ( near * far ) / ( far - perspective_depth * ( far - near ) );
}

float LinearDepth01( float perspective_depth )
{
    float near = camera_buffer_data.camera_constants.a;
    float far = camera_buffer_data.camera_constants.b;

    float linear_depth = LinearDepth( perspective_depth, near, far );
    return ( linear_depth - near ) / ( far - near );
}

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    VertexOutput output;
    output.sv_position = float4( input.position, 0.1f, 1.f );
    output.uv = ( input.position + 1.f ) * 0.5f;
    return output;
}

[shader( "fragment" )]
float4 fs_main( VertexOutput in )
    : SV_Target
{
    float3 in_pos = GBuffer_Position.Sample( nearest_sampler, in.uv ).rgb;
    float3 in_nor = GBuffer_Normals.Sample( nearest_sampler, in.uv ).rgb;
    float4 in_tangent = GBuffer_Position.Sample( nearest_sampler, in.uv );
    float2 in_uv = GBuffer_UV.Sample( nearest_sampler, in.uv ).rg;

    float stencil = GBuffer_Position.Sample( nearest_sampler, in.uv ).a;
    float2 ddx_uv = float2( GBuffer_Position.Sample( nearest_sampler, in.uv ).a,
        GBuffer_Normals.Sample( nearest_sampler, in.uv ).a );

    float2 ddy_uv = GBuffer_UV.Sample( nearest_sampler, in.uv ).ba;

    if ( length( in_nor ) == 0.0f ) {
        return float4( 0.0f );
    }

    // Texture samples
    float3 base_albedo = debug_data.enable_albedo_map
        ? GBuffer_Albedo.Sample( nearest_sampler, in.uv ).rgb
        : debug_data.color.rgb;

    float3 normal_map_norm = normal_map.Sample( nearest_sampler, in.uv ).rgb;

    float2 metallic_roughness = debug_data.enable_roughness_metal_map
        ? GBuffer_Packed_Data.Sample( nearest_sampler, in.uv ).gb
        : float2( debug_data.packed_data0.x, debug_data.packed_data0.y );

    // Material properties
    float metallic = metallic_roughness.y;
    float roughness = metallic_roughness.x;

    // Basic vectors
    float3 light = normalize( debug_data.sun_direction.rgb );
    float3 radiance = float3( 1, 1, 1 );
    // float attenuation = 1.0f / length( light_pos - in_pos );
    // attenuation *= attenuation;

    float3 camera_pos = camera_buffer_data.camera_pos.rgb;
    float3 normal = debug_data.enable_normal_map
        ? map_normals( normal_map_norm, in_tangent, in_nor )
        : normalize( in_nor );
    float3 view = normalize( camera_pos - in_pos );
    float3 half = normalize( view + light );

    Material pbr = {
        roughness, metallic,
        debug_data.packed_data0.z, // cc roughness
        debug_data.packed_data0.w // cc weight
    };

    GBuffer gbuffer = { in_pos, normal, in_tangent, in_uv, ddx_uv, ddy_uv };

    float3 out_color = car_bsdf( normal, view, half, light, pbr, gbuffer );

    return float4( out_color, 1.0f );
}


