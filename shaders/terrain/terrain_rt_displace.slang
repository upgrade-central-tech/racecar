#include "../common.slang"
#include "../utils.slang"
#include "../clouds/cloud_utils.slang"
#include "terrain_common.slang"

struct TerrainRTGrid {
    float2 world_min;
    float2 world_size;
    uint2 vertex_count;
    float2 _pad;
};

struct TerrainRTVertex {
    float3 position;
    float _pad0;
    float3 normal;
    float _pad1;
};

layout( binding = 0, set = 0 ) ConstantBuffer<TerrainData> terrain_data;
layout( binding = 1, set = 0 ) ConstantBuffer<TerrainRTGrid> grid;

layout( binding = 0, set = 1 ) Texture2D<float2> layer_test_mask;

layout( binding = 0, set = 2 ) SamplerState linear_sampler;

layout( binding = 0, set = 3 ) RWStructuredBuffer<TerrainRTVertex> out_vertices;

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void cs_terrain_rt_displace( uint3 thread_id: SV_DispatchThreadID )
{
    if ( thread_id.x >= grid.vertex_count.x || thread_id.y >= grid.vertex_count.y ) {
        return;
    }

    float2 grid_t = float2( thread_id.xy ) / float2( grid.vertex_count - uint2( 1 ) );
    float2 world_xz = grid.world_min + grid_t * grid.world_size;

    float3 base_position = float3( world_xz.x, 0.0f, world_xz.y );

    float2 scroll = terrain_data.terrain_data1.xy;

    float2 spacing = grid.world_size / float2( grid.vertex_count - uint2( 1 ) );

    TerrainRTVertex vertex;
    vertex.position = terrain_displace( base_position, scroll, layer_test_mask, linear_sampler );
    vertex.normal = terrain_height_normal(
        base_position, scroll, layer_test_mask, linear_sampler, spacing.x );
    vertex._pad0 = 0.0f;
    vertex._pad1 = 0.0f;

    out_vertices[thread_id.y * grid.vertex_count.x + thread_id.x] = vertex;
}
