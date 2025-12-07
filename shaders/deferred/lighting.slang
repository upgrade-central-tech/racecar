#include "../common.slang"

struct VertexInput {
    float2 position : POSITION;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    float2 uv;
};

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

// Cubemaps + LUTs
layout( binding = 0, set = 2 ) Texture2D<float2> BRDF_LUT;
layout( binding = 1, set = 2 ) Texture2D<float4> glint_noise;
layout( binding = 2, set = 2 ) Texture2D<float4> octahedral_sky;
layout( binding = 3, set = 2 ) Texture2D<float4> octahedral_sky_irradiance;
layout( binding = 4, set = 2 ) Texture2D<float4> octahedral_sky_mips;

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

layout( binding = 0, set = 6 ) Texture2D<float4> ReflectionData;

// #include "../utils.slang"
// #include "../glint/glint_noise_init.hlsl"
// #include "../glint/glint.hlsl"

#include "../car_mat/car_lighting.slang"

bool traceRayFirstHit( RaytracingAccelerationStructure sceneBVH, float3 rayOrigin, float3 rayDir,
    out float t, out int primitiveIndex )
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

    q.TraceRayInline( sceneBVH, rayFlags, 0xff, ray );
    q.Proceed();

    if ( q.CommittedStatus() == COMMITTED_TRIANGLE_HIT ) {
        t = q.CommittedRayT();
        primitiveIndex = q.CommittedPrimitiveIndex();
        return true;
    }
    primitiveIndex = q.CandidatePrimitiveIndex();
    unused( t );
    return false;
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
    float4 refl_data = ReflectionData.Sample( nearest_sampler, in.uv );

    return refl_data;

    float stencil = GBuffer_Position.Sample( nearest_sampler, in.uv ).a;
    float2 ddx_uv = float2( GBuffer_Position.Sample( nearest_sampler, in.uv ).a,
        GBuffer_Normals.Sample( nearest_sampler, in.uv ).a );

    float2 ddy_uv = GBuffer_UV.Sample( nearest_sampler, in.uv ).ba;

    if ( stencil != CAR_ID ) {
        return float4( 0.0f );
    }

    // Texture samples
    float3 base_albedo = GBuffer_Albedo.Sample( nearest_sampler, in.uv ).rgb;

    float4 metallic_roughness = GBuffer_Packed_Data.Sample( nearest_sampler, in.uv );

    // Material properties
    float metallic = metallic_roughness.y;
    float roughness = metallic_roughness.x;

    // Basic vectors
    float3 light = normalize( debug_data.sun_direction.rgb );
    float3 radiance = float3( 1, 1, 1 );
    // float attenuation = 1.0f / length( light_pos - in_pos );
    // attenuation *= attenuation;

    float3 camera_pos = camera_buffer_data.camera_pos.rgb;
    float3 normal = normalize( in_nor );
    float3 view = normalize( camera_pos - in_pos );
    float3 half = normalize( view + light );

    Material pbr = {
        roughness, metallic,
        metallic_roughness.z, // cc roughness
        metallic_roughness.w // cc weight
    };

    GBuffer gbuffer = { in_pos, normal, in_tangent, in_uv, ddx_uv, ddy_uv };

    bool hit = false;

    if ( debug_data.ray_traced_shadows ) {
        float t;
        int primitive_index;
        hit = traceRayFirstHit(
            sceneBVH, in_pos + 0.01 * normalize( in_nor ), light, t, primitive_index );
    }

    float3 out_color
        = car_bsdf( base_albedo, normal, view, half, light, pbr, gbuffer, hit ? 0.0 : 1.0 );

    return float4( out_color, 1.0f );
}


