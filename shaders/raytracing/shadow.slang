
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

layout( binding = 0, set = 5 ) RaytracingAccelerationStructure sceneBVH;

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    VertexOutput output;
    output.sv_position = float4( input.position, 0.1f, 1.f );
    output.uv = ( input.position + 1.f ) * 0.5f;
    return output;
}

bool traceRayFirstHit(
    RaytracingAccelerationStructure sceneBVH,
    float3 rayOrigin,
    float3 rayDir,
    out float t,
    out int primitiveIndex )
{
    RayDesc ray;
    ray.Origin = rayOrigin;
    ray.TMin = 0.001f;
    ray.Direction = rayDir;
    ray.TMax = 1e4f;
    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
            //  RAY_FLAG_CULL_BACK_FACING_TRIANGLES |
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;

    let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
            //  RAY_FLAG_CULL_BACK_FACING_TRIANGLES |
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

    q.TraceRayInline(
        sceneBVH,
        rayFlags,
        0xff,
        ray );
    q.Proceed();

    if ( q.CommittedStatus() == COMMITTED_TRIANGLE_HIT )
    {
        t = q.CommittedRayT();
        primitiveIndex = q.CommittedPrimitiveIndex();
        return true;
    }
    primitiveIndex = q.CandidatePrimitiveIndex();
    unused( t );
    return false;
}

[shader( "fragment" )]
float4 fs_main( VertexOutput in )
    : SV_Target
{
    float3 in_pos = GBuffer_Position.Sample( nearest_sampler, in.uv ).rgb;
    float3 in_nor = GBuffer_Normals.Sample( nearest_sampler, in.uv ).rgb;
    float4 in_tangent = GBuffer_Tangent.Sample( nearest_sampler, in.uv );
    float2 in_uv = GBuffer_UV.Sample( nearest_sampler, in.uv ).rg;

    float3 light = normalize( debug_data.sun_direction.rgb );

    if ( length( in_nor ) == 0.0f ) {
        return float4( 0.0f );
    }

    float t;
    int primitive_index;

    bool hit = traceRayFirstHit(
        sceneBVH,
        in_pos + 0.01 * normalize( in_nor ),
        // in_pos,
        light,
        t,
        primitive_index
    );

    float3 base_albedo = debug_data.enable_albedo_map
        ? GBuffer_Albedo.Sample( nearest_sampler, in.uv ).rgb
        : debug_data.color.rgb;

    float shadow = 0.7;
    if ( hit ) {
        shadow = 0.0;
    }

    float3 out_color = base_albedo * ( max( dot( light, normalize( in_nor ) ), 0.0 ) * shadow + 0.01 );

    return float4( out_color, 1.0f );
}


