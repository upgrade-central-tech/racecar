#include "../common.slang"

struct VertexInput {
    float2 position : POSITION;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    float2 uv;
};

// It's an int array, but we use uint4 for packing
// 64 * 4 == MAX_RT_PRIMITIVES in src/engine/ub_data.hpp.
struct BLASOffsets {
    uint4 vertex_buffer_offset[64];
    uint4 index_buffer_offset[64];
    uint4 material_id[64];
};

struct RTTextureUniform {
    int4 albedo_texture_index[64];
    int4 metallic_roughness_texture_index[64];
    int4 normal_texture_index[64];
};

struct PaddedVertex {
    float4 position; // float3
    float4 normal; // float3
    float4 tangent; // float4
    float4 uv; // float2
};

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_buffer_data;
layout( binding = 1, set = 0 ) ConstantBuffer<DebugData> debug_data;

layout( binding = 0, set = 1 ) SamplerState nearest_sampler;
layout( binding = 1, set = 1 ) SamplerState point_sampler;

layout( binding = 0, set = 2 ) Texture2D<float4> GBuffer_Position;
layout( binding = 1, set = 2 ) Texture2D<float4> GBuffer_Normals;
layout( binding = 2, set = 2 ) Texture2D<float4> GBuffer_Tangent;
layout( binding = 3, set = 2 ) Texture2D<float4> GBuffer_UV;
layout( binding = 4, set = 2 ) Texture2D<float4> GBuffer_Albedo;
layout( binding = 5, set = 2 ) Texture2D<float> GBuffer_Depth;
layout( binding = 6, set = 2 ) Texture2DMS<float> GBuffer_DepthMS;
layout( binding = 7, set = 2 ) Texture2D<float4> GBuffer_Packed_Data;

#if RACECAR_RAY_TRACING
layout( binding = 0, set = 3 ) RaytracingAccelerationStructure sceneBVH;
layout( binding = 0, set = 4 ) RaytracingAccelerationStructure terrainBVH;
#endif // RACECAR_RAY_TRACING

layout( binding = 0, set = 5 ) StructuredBuffer<PaddedVertex> vertex_data;
layout( binding = 1, set = 5 ) StructuredBuffer<uint32_t> index_data;
layout( binding = 2, set = 5 ) ConstantBuffer<BLASOffsets> blas_offsets;
layout( binding = 3, set = 5 ) ConstantBuffer<RTTextureUniform> rt_texture_uniform;
layout( binding = 4, set = 5 ) Texture2D<float2> BRDF_LUT;
layout( binding = 5, set = 5 ) Texture2D<float4> octahedral_sky_mips;
layout( binding = 6, set = 5 ) Texture2D<float4> octahedral_sky_irradiance;
layout( binding = 7, set = 5 ) ConstantBuffer<MaterialTable> material_table;

layout( binding = 0, set = 6 ) Texture2D<float4> albedo_textures[];
layout( binding = 1, set = 6 ) Texture2D<float4> metallic_roughness_textures[];

#include "../car_mat/car_lighting.slang"

#if RACECAR_RAY_TRACING
static const float3x4 IDENTITY_3X4
    = float3x4( 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 );

bool traceRayClosestHit( float stencil, float3 rayOrigin, float3 rayDir, out float t,
    out int primitiveIndex, out float2 barycentrics, out int instanceIndex,
    out float3x4 objectToWorld, out float3x4 worldToObject )
{
    RayDesc ray;
    ray.Origin = rayOrigin;
    ray.TMin = 0.001f;
    ray.Direction = rayDir;
    ray.TMax = 1e4f;

    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_CULL_BACK_FACING_TRIANGLES> q;

    uint rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_CULL_BACK_FACING_TRIANGLES;

    // Terrain pixels reflect the car, and car pixels reflect the car onto itself. Both cases
    // trace the car TLAS, so both can be resolved through the same car hit shading path.
    if ( stencil == TERRAIN_ID || stencil == CAR_ID ) {
        q.TraceRayInline( sceneBVH, rayFlags, 0xff, ray );
    } else {
        q.TraceRayInline( terrainBVH, rayFlags, 0xff, ray );
    }

    while ( q.Proceed() ) { }

    if ( q.CommittedStatus() == COMMITTED_TRIANGLE_HIT ) {
        t = q.CommittedRayT();
        primitiveIndex = q.CommittedPrimitiveIndex();
        instanceIndex = q.CommittedInstanceIndex();
        barycentrics = q.CommittedTriangleBarycentrics();
        objectToWorld = q.CommittedObjectToWorld3x4();
        worldToObject = q.CommittedWorldToObject3x4();
        return true;
    }

    t = 0.0f;
    primitiveIndex = -1;
    instanceIndex = -1;
    barycentrics = float2( 0, 0 );
    objectToWorld = IDENTITY_3X4;
    worldToObject = IDENTITY_3X4;
    return false;
}
#endif // RACECAR_RAY_TRACING

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
    : SV_Target0
{
    float3 in_pos = GBuffer_Position.Sample( nearest_sampler, in.uv ).rgb;
    float3 in_nor = GBuffer_Normals.Sample( nearest_sampler, in.uv ).rgb;

    float stencil = GBuffer_Position.Sample( nearest_sampler, in.uv ).a;

    RaytracingAccelerationStructure accel_structure;

    if ( stencil == 0 ) {
        return float4( asfloat( -1 ), 0.0, 0.0, 0.0 );
    }

    float3 world_dir = in_pos - camera_buffer_data.camera_pos.xyz;
    float3 view_dir = normalize( world_dir );
    float3 refl_dir = reflect( view_dir, in_nor );

#if RACECAR_RAY_TRACING
    float t;
    int prim_idx;
    int instance_idx;
    float2 barycentrics;
    float3x4 object_to_world;
    float3x4 world_to_object;
    bool hit = traceRayClosestHit( stencil, in_pos, refl_dir, t, prim_idx, barycentrics,
        instance_idx, object_to_world, world_to_object );

    if ( !hit ) {
        return float4( 0.0 );
    }
    
    // return float4( float( instance_idx ) / 40.0f, 1.0 - float( instance_idx ) / 40.0f,
    // 0.0, 1.0 );
    uint4 idx_buf = blas_offsets.index_buffer_offset[instance_idx / 4];
    uint4 vtx_buf = blas_offsets.vertex_buffer_offset[instance_idx / 4];

    uint32_t index_offset = idx_buf[instance_idx % 4];
    uint32_t vertex_offset = vtx_buf[instance_idx % 4];

    int idx_1 = index_data[index_offset + 3 * prim_idx];
    int idx_2 = index_data[index_offset + 3 * prim_idx + 1];
    int idx_3 = index_data[index_offset + 3 * prim_idx + 2];

    PaddedVertex col1 = vertex_data[vertex_offset + idx_1];
    PaddedVertex col2 = vertex_data[vertex_offset + idx_2];
    PaddedVertex col3 = vertex_data[vertex_offset + idx_3];

    float3 local_barycentrics
        = float3( 1.0f - barycentrics.x - barycentrics.y, barycentrics.x, barycentrics.y );

    float3 object_position = ( col1.position.xyz * local_barycentrics.x )
        + ( col2.position.xyz * local_barycentrics.y )
        + ( col3.position.xyz * local_barycentrics.z );

    float3 position = mul( object_to_world, float4( object_position, 1.0f ) );

    float2 uv = ( col1.uv.xy * local_barycentrics.x ) + ( col2.uv.xy * local_barycentrics.y )
        + ( col3.uv.xy * local_barycentrics.z );

    float3 object_normal = ( col1.normal.xyz * local_barycentrics.x )
        + ( col2.normal.xyz * local_barycentrics.y ) + ( col3.normal.xyz * local_barycentrics.z );

    float3x3 world_to_object_3x3
        = float3x3( world_to_object[0].xyz, world_to_object[1].xyz, world_to_object[2].xyz );

    float3 normal = normalize( mul( object_normal, world_to_object_3x3 ) );

    int albedo_texture_index
        = rt_texture_uniform.albedo_texture_index[instance_idx / 4][instance_idx % 4];
    int metallic_roughness_texture_index
        = rt_texture_uniform.metallic_roughness_texture_index[instance_idx / 4][instance_idx % 4];

    uint material_id = blas_offsets.material_id[instance_idx / 4][instance_idx % 4];
    MaterialData material = material_table.materials[material_id];

    float3 albedo = material.base_color.rgb;
    float roughness = material.roughness;
    float metallic = material.metallic;
    float clearcoat_roughness = material.clearcoat_roughness;
    float clearcoat_weight = material.clearcoat;

    if ( albedo_texture_index != -1 ) {
        float4 sam = albedo_textures[albedo_texture_index].Sample( nearest_sampler, uv );
        albedo = sam.rgb;
    }

    if ( metallic_roughness_texture_index != -1 ) {
        float4 sam = metallic_roughness_textures[metallic_roughness_texture_index].Sample(
            nearest_sampler, uv );
        roughness = sam.y;
        metallic = sam.z;
    }

    GBuffer g_buffer = {
        in_pos,
        in_nor,
        float4( 0.0f ), // tangents
        float2( 0.0f ), // uv
        float2( 0.0f ), // ddx_uv
        float2( 0.0f ), // ddy_uv
    };

    Material pbr = {
        roughness,
        metallic,
        clearcoat_roughness,
        clearcoat_weight,
    };

    float3 light = normalize( debug_data.sun_direction ).rgb;
    float3 hit_view_dir = -refl_dir;
    float3 half_vector = normalize( hit_view_dir + light );

    //
    float4 refl_input
        = float4( 0.0f ); // just use 1-bounce for reflections, sample IBL - mark alpha 0, as such
    float3 color = car_bsdf(
        albedo, normal, hit_view_dir, half_vector, light, pbr, g_buffer, 1.0f, refl_input );

    return float4( color, 1.0f );

    // int roughness_metallic_index =
    // rt_texture_uniform.metallic_roughness_texture_index[instance_idx / 4][instance_idx %
    // 4];

    // if ( roughness_metallic_index == -1 ) {
    //     return float4( rt_texture_uniform.roughness[instance_idx / 4][instance_idx % 4],
    //     rt_texture_uniform.metallic[instance_idx / 4][instance_idx % 4], 0.0, 1.0);
    // }
    // else {
    //     return float4( 0.0, 1.0, 0.0, 1.0 );
    //     float4 sample = metallic_roughness_textures[roughness_metallic_index].Sample(
    //     nearest_sampler, uv ); return float4( sample.rgb, 1.0 );
    // }

    // return float4( normalize(color), 1.0 );
#else
    return float4( 0.0 );
#endif // RACECAR_RAY_TRACING
}


