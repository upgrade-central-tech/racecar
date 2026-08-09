#include "../common.slang"

#include "../terrain/terrain_common.slang"

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
// Car AS
layout( binding = 0, set = 3 ) RaytracingAccelerationStructure sceneBVH;
#endif // RACECAR_RAY_TRACING

layout( binding = 0, set = 4 ) StructuredBuffer<PaddedVertex> vertex_data;
layout( binding = 1, set = 4 ) StructuredBuffer<uint32_t> index_data;
layout( binding = 2, set = 4 ) ConstantBuffer<BLASOffsets> blas_offsets;
layout( binding = 3, set = 4 ) ConstantBuffer<RTTextureUniform> rt_texture_uniform;
layout( binding = 4, set = 4 ) Texture2D<float2> BRDF_LUT;
layout( binding = 5, set = 4 ) Texture2D<float4> octahedral_sky_mips;
layout( binding = 6, set = 4 ) Texture2D<float4> octahedral_sky_irradiance;
layout( binding = 7, set = 4 ) ConstantBuffer<MaterialTable> material_table;

layout( binding = 0, set = 5 ) Texture2D<float4> albedo_textures[];
layout( binding = 1, set = 5 ) Texture2D<float4> metallic_roughness_textures[];

#if RACECAR_RAY_TRACING
layout( binding = 0, set = 6 ) RaytracingAccelerationStructure terrainBVH;
layout( binding = 1, set = 6 ) ConstantBuffer<TerrainData> terrain_data;
layout( binding = 2, set = 6 ) Texture2D<float2> terrain_layer_mask;
layout( binding = 3, set = 6 ) Texture2D<float4> terrain_grass_albedo_roughness;
layout( binding = 4, set = 6 ) Texture2D<float4> terrain_grass_normal_ao;
layout( binding = 5, set = 6 ) Texture2D<float4> terrain_asphalt_albedo_roughness;
layout( binding = 6, set = 6 ) Texture2D<float4> terrain_asphalt_normal_ao;
layout( binding = 7, set = 6 ) SamplerState terrain_linear_sampler;
#endif // RACECAR_RAY_TRACING

#include "../car_mat/car_lighting.slang"

#if RACECAR_RAY_TRACING

// matches terrain spacing in terrain.cpp/terrain_rt.cpp
static const float TERRAIN_RT_NORMAL_EPS = 500.0f / 255.0f;

static const float3x4 IDENTITY_3X4
    = float3x4( 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 );

bool traceRayClosestHit( RaytracingAccelerationStructure bvh, float3 rayOrigin, float3 rayDir,
    out float t, out int primitiveIndex, out float2 barycentrics, out int instanceIndex,
    out float3x4 objectToWorld, out float3x4 worldToObject )
{
    RayDesc ray;
    ray.Origin = rayOrigin;
    ray.TMin = 0.001f;
    ray.Direction = rayDir;
    ray.TMax = 1e4f;

    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_CULL_BACK_FACING_TRIANGLES> q;

    uint rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_CULL_BACK_FACING_TRIANGLES;

    q.TraceRayInline( bvh, rayFlags, 0xff, ray );

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

bool traceTerrainClosestHit( float3 rayOrigin, float3 rayDir, out float t )
{
    RayDesc ray;
    ray.Origin = rayOrigin;
    ray.TMin = 0.001f;
    ray.Direction = rayDir;
    ray.TMax = 1e4f;

    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;

    uint rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES;

    q.TraceRayInline( terrainBVH, rayFlags, 0xff, ray );

    while ( q.Proceed() ) { }

    if ( q.CommittedStatus() == COMMITTED_TRIANGLE_HIT ) {
        t = q.CommittedRayT();
        return true;
    }

    t = 0.0f;
    return false;
}

TerrainMaps get_terrain_maps()
{
    TerrainMaps maps;
    maps.layer_mask = terrain_layer_mask;
    maps.grass_albedo_roughness = terrain_grass_albedo_roughness;
    maps.grass_normal_ao = terrain_grass_normal_ao;
    maps.asphalt_albedo_roughness = terrain_asphalt_albedo_roughness;
    maps.asphalt_normal_ao = terrain_asphalt_normal_ao;
    maps.linear_sampler = terrain_linear_sampler;
    return maps;
}

float3 shade_terrain_hit( float3 world_position, float3 ray_dir )
{
    float2 scroll = terrain_data.terrain_data1.xy;
    float snowness = terrain_data.terrain_data0.z;
    float wetness_toggle = terrain_data.terrain_data0.y;

    float3 base_position = float3( world_position.x, 0.0f, world_position.z );
    float3 vertex_normal = terrain_height_normal(
        base_position, scroll, terrain_layer_mask, terrain_linear_sampler, TERRAIN_RT_NORMAL_EPS );

    TerrainUVs uvs = terrain_compute_uvs( world_position, scroll );

    TerrainSurface surface = terrain_evaluate_surface( world_position, vertex_normal, scroll, uvs,
        snowness, wetness_toggle, get_terrain_maps(), terrain_no_gradients() );

    float3 albedo = surface.albedo;
    float roughness = surface.roughness;
    float3 normal = surface.normal;
    float ao = surface.ao;

    terrain_apply_wetness_exposure( wetness_toggle, albedo, roughness );

    float3 light = normalize( debug_data.sun_direction.rgb );
    float3 view = -ray_dir;
    float3 half_vector = normalize( view + light );

    BSDF_Dots dots = get_dots( normal, view, half_vector, light );
    float n_dot_h_sq = get_NoH_squared( dots.n_dot_l, dots.n_dot_v, dots.v_dot_l, 0.035f );
    float NDF = min( get_GGX_from_n_dot_h_sq( n_dot_h_sq, roughness * roughness ), 65.0f );

    float3 color
        = terrain_direct_lighting( albedo, normal, light, NDF, roughness, ao, view, terrain_data );

    float2 oct_uv = oct_encode( normal );
    float2 oct_refl_uv = oct_encode( normalize( reflect( -view, normal ) ) );

    float3 diffuse_irradiance = filter_float4( octahedral_sky_irradiance, oct_uv ).rgb;
    float3 specular_irradiance = terrain_sample_glossy_ibl(
        roughness, oct_refl_uv, octahedral_sky_mips, terrain_linear_sampler );

    float n_dot_v = max( dot( normal, view ), 0.0f );
    float3 F0 = lerp( float3( 0.04f ), albedo, 0.04f );
    float2 BRDF = BRDF_LUT.SampleLevel( terrain_linear_sampler, float2( n_dot_v, roughness ), 0.0f );
    specular_irradiance *= ( F0 * BRDF.x + BRDF.y );

    const float irradiance_strength = 0.5f;
    color += ( albedo * TERRAIN_INV_PI ) * diffuse_irradiance * irradiance_strength;
    color += clamp( specular_irradiance, float3( 0.0f ), float3( 1.0f ) );

    return color;
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
    bool hit = traceRayClosestHit( sceneBVH, in_pos, refl_dir, t, prim_idx, barycentrics,
        instance_idx, object_to_world, world_to_object );

    float terrain_t = 0.0f;
    bool terrain_hit = false;

    if ( stencil == CAR_ID ) {
        terrain_hit = traceTerrainClosestHit( in_pos, refl_dir, terrain_t );
    }

    if ( terrain_hit && ( !hit || terrain_t < t ) ) {
        float3 terrain_position = in_pos + terrain_t * refl_dir;
        return float4( shade_terrain_hit( terrain_position, refl_dir ), 1.0f );
    }

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


