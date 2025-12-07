#include "../common.slang"

struct VertexInput {
    float2 position : POSITION;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    float2 uv;
};

struct BLASOffsets {
    uint4 vertex_buffer_offset[26];
    uint4 index_buffer_offset[26];
};

struct RTTextureUniform {
    float4 base_color[104];
    int4 albedo_texture_index[26];

    float4 metallic[26];
    float4 roughness[26];
    int4 metallic_roughness_texture_index[26];

    int4 normal_texture_index[26];
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

layout( binding = 0, set = 3 ) RaytracingAccelerationStructure sceneBVH;
layout( binding = 0, set = 4 ) RaytracingAccelerationStructure terrainBVH;

layout( binding = 0, set = 5 ) StructuredBuffer<PaddedVertex> vertex_data;
layout( binding = 1, set = 5 ) StructuredBuffer<uint32_t> index_data;
layout( binding = 2, set = 5 ) ConstantBuffer<BLASOffsets> blas_offsets;
layout( binding = 3, set = 5 ) ConstantBuffer<RTTextureUniform> rt_texture_uniform;
layout( binding = 4, set = 5 ) Texture2D<float2> BRDF_LUT;
layout( binding = 5, set = 5 ) Texture2D<float4> octahedral_sky_mips;
layout( binding = 6, set = 5 ) Texture2D<float4> octahedral_sky_irradiance;

layout( binding = 0, set = 6 ) Texture2D<float4> albedo_textures[];
layout( binding = 1, set = 6 ) Texture2D<float4> metallic_roughness_textures[];

#include "../car_mat/car_lighting.slang"

bool traceRayClosestHit( float stencil, float3 rayOrigin, float3 rayDir, out float t,
    out int primitiveIndex, out float2 barycentrics, out int instanceIndex )
{
    RayDesc ray;
    ray.Origin = rayOrigin;
    ray.TMin = 0.001f;
    ray.Direction = rayDir;
    ray.TMax = 1e4f;

    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_CULL_BACK_FACING_TRIANGLES> q;

    uint rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_CULL_BACK_FACING_TRIANGLES;

    if ( stencil == TERRAIN_ID ) {
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
        return true;
    }

    t = 0.0f;
    primitiveIndex = -1;
    instanceIndex = -1;
    barycentrics = float2( 0, 0 );
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

    float t;
    int prim_idx;
    int instance_idx;
    float2 barycentrics;
    bool hit
        = traceRayClosestHit( stencil, in_pos, refl_dir, t, prim_idx, barycentrics, instance_idx );

    if ( hit ) {
        if ( stencil == CAR_ID ) {
            return float4( 0.0 );
        } else {
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
                = float3( barycentrics, 1.0 - barycentrics.x - barycentrics.y );

            float3 position = ( col1.position.xyz * local_barycentrics.x )
                + ( col2.position.xyz * local_barycentrics.y )
                + ( col3.position.xyz * local_barycentrics.z );

            float2 uv = ( col1.uv.xy * local_barycentrics.x )
                + ( col2.uv.xy * local_barycentrics.y ) + ( col3.uv.xy * local_barycentrics.z );

            float3 normal = normalize( ( col1.normal.xyz * local_barycentrics.x )
                + ( col2.normal.xyz * local_barycentrics.y )
                + ( col3.normal.xyz * local_barycentrics.z ) );

            int albedo_texture_index
                = rt_texture_uniform.albedo_texture_index[instance_idx / 4][instance_idx % 4];
            int metallic_roughness_texture_index
                = rt_texture_uniform
                      .metallic_roughness_texture_index[instance_idx / 4][instance_idx % 4];

            float3 albedo;
            float roughness = 0.1f;
            float metallic = 0.0f;
            float clearcoat_roughness = 0.5f;
            float clearcoat_weight = 0.0f;

            if ( albedo_texture_index == -1 ) {
                albedo = ( rt_texture_uniform.base_color[instance_idx].rgb );
            } else {
                float4 sample = albedo_textures[albedo_texture_index].Sample( nearest_sampler, uv );
                albedo = sample.rgb;
            }

            if ( metallic_roughness_texture_index == -1 ) {
                // metallic = rt_texture_uniform.metallic[instance_idx].r;
                // roughness = rt_texture_uniform.roughness[instance_idx].r;
            } else {
                float4 sample
                    = metallic_roughness_textures[metallic_roughness_texture_index].Sample(
                        nearest_sampler, uv );
                // metallic = sample.x;
                // roughness = sample.y;
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
            float3 view_dir = -refl_dir;
            float3 half_vector = normalize( view_dir + light );

            //
            float4 refl_input = float4(
                0.0f ); // just use 1-bounce for reflections, sample IBL - mark alpha 0, as such
            float3 color = car_bsdf(
                albedo, normal, view_dir, half_vector, light, pbr, g_buffer, 1.0f, refl_input );

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
        }
    } else {
        return float4( 0.0 );
    }
}


