#include "../common.slang"

struct VertexInput {
    float2 position : POSITION;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    float2 uv;
};

struct BLASOffsets {
    uint32_t vertex_buffer_offset[100];
    uint32_t index_buffer_offset[100];
};

struct Vertex {
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv;
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

layout( binding = 0, set = 5 ) StructuredBuffer<Vertex> vertex_data;
layout( binding = 1, set = 5 ) StructuredBuffer<uint32_t> index_data;
layout( binding = 2, set = 5 ) ConstantBuffer<BLASOffsets> blas_offsets;


bool traceRayClosestHit( RaytracingAccelerationStructure sceneBVH, float3 rayOrigin, float3 rayDir,
    out float t, out int primitiveIndex, out float2 barycentrics )
{
    RayDesc ray;
    ray.Origin = rayOrigin;
    ray.TMin = 0.001f;
    ray.Direction = rayDir;
    ray.TMax = 1e4f;

    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_CULL_BACK_FACING_TRIANGLES> q;

    uint rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_CULL_BACK_FACING_TRIANGLES;

    q.TraceRayInline( sceneBVH, rayFlags, 0xff, ray );

    while ( q.Proceed() ) { }

    if ( q.CommittedStatus() == COMMITTED_TRIANGLE_HIT ) {
        t = q.CommittedRayT();
        primitiveIndex = q.CommittedPrimitiveIndex();
        barycentrics = q.CommittedTriangleBarycentrics();
        return true;
    }

    t = 0.0f;
    primitiveIndex = -1;
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
    else if ( stencil == TERRAIN_ID ) {
        accel_structure = sceneBVH;
        // return float4( 1.0, 0.0, 0.0, 1.0 );
    }
    else if ( stencil == CAR_ID ) {
        accel_structure = terrainBVH;
        // return float4( 0.0, 1.0, 0.0, 1.0 );
    }

    float3 world_dir = in_pos - camera_buffer_data.camera_pos.xyz;
    float3 norm_world_dir = normalize( world_dir );
    float3 refl_dir = reflect( norm_world_dir, in_nor );

    float t;
    int prim_idx;
    float2 barycentrics;
    bool hit = traceRayClosestHit( accel_structure, in_pos, refl_dir, t, prim_idx, barycentrics );

    if ( hit ) {
        if ( stencil == CAR_ID ) {
            return float4( float( prim_idx ) / 40.0f, 1.0 - float( prim_idx ) / 40.0f, 0.0, 1.0 );
        } else {
            return float4( float( prim_idx ) / 40.0f, 1.0 - float( prim_idx ) / 40.0f, 0.0, 1.0 );
        }
    } else {
        return float4( asfloat( -1 ), 1.0, 0.0, 0.0 );
    }
}


