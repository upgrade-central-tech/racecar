struct VertexInput {
    float4 color : COLOR0;
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT0;
    float2 uv : TEXCOORD0;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    float3 position;
    float3 normal;
    float4 tangent;
    float3 color;
    float2 uv;
};

struct CameraBufferData {
    float4x4 mvp;
    float4x4 model;
    float4x4 inv_model;
    float3 camera_pos;
    float3 color;
};

struct RaymarchBufferData {
    float step_size;
};

struct DebugData {
    bool enable_albedo_map;
    bool enable_normal_map;
    bool enable_roughness_metal_map;
    bool normals_only;
    bool albedo_only;
    bool roughness_metal_only;
}

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_buffer_data;
layout( binding = 1, set = 0 ) ConstantBuffer<DebugData> debug_data;

// Nothing used in set 1 currently.

layout( binding = 0, set = 2 ) Texture3D<float> volume_data;

layout( binding = 0, set = 3 ) SamplerState nearest_sampler;

static const float PI = 3.14159265359;

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    VertexOutput output;
    output.sv_position = mul( camera_buffer_data.mvp, float4( input.position, 1.0 ) );
    output.position = mul( camera_buffer_data.model, float4( input.position, 1.0 ) ).xyz;
    output.color = input.position.xyz;

    float3x3 normal_matrix = (float3x3)transpose( camera_buffer_data.inv_model );
    float3 normal = normalize( mul( normal_matrix, input.normal ) );
    float3 tangent = normalize( mul( normal_matrix, input.tangent.xyz ) );
    tangent = normalize( tangent - normal * dot( normal, tangent ) );

    output.normal = normal;
    output.tangent = float4( tangent, input.tangent.w );
    output.uv = input.uv;

    return output;
}

float3 get_texture_position( float3 local_position )
{
    // Remap from [-1,1] to [0,1].
    return ( local_position * 0.5f ) + 0.5f;
}

// https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection.html
bool check_ray_bbox_intersection( float3 local_origin, float3 local_dir, float3 minAABB,
    float3 maxAABB, out float t_min, out float t_max )
{
    float3 inv_dir = 1.0f / local_dir;

    float3 t_low = ( minAABB - local_origin ) * inv_dir;
    float3 t_high = ( maxAABB - local_origin ) * inv_dir;

    float3 t_close = min( t_low, t_high );
    float3 t_far = max( t_low, t_high );

    t_min = max( t_close.x, max( t_close.y, t_close.z ) );
    t_max = min( t_far.x, max( t_far.y, t_far.z ) );

    return max( 0.0f, t_min ) <= t_max;
}

float raymarch( float3 view, float3 origin )
{
    float3 local_dir = normalize( mul( (float3x3)camera_buffer_data.inv_model, view ) );
    float3 local_origin = mul( camera_buffer_data.inv_model, float4( origin, 1.0f ) ).xyz;

    // Raymarched space is now in, unfortunately, [-1,1] coordinates - maintained in "object space".
    // To sample our 3D texture with UVs in [0, 1], we need the dimensions of our 3D object.

    // Hard-coded cube bounds, this would be ideally passed in via a min/max AABB finder in a GLB
    // loader or something.
    float3 minAABB = float3( -1, -1, -1 );
    float3 maxAABB = float3( 1, 1, 1 );

    float t_min, t_max;
    if ( !check_ray_bbox_intersection( local_origin, local_dir, minAABB, maxAABB, t_min, t_max ) ) {
        return 0.0f;
    }

    float t = 0.0f;
    float step_size = 0.10f;
    float accumulated = 0.0f;

    float3 sphere_origin = float3( 0 );
    float sphere_radius = 0.5f;

    for ( int step = 0; step < 20; step++ ) {
        float3 marched_pos = local_origin + local_dir * t;

        float3 sample_uv = get_texture_position( marched_pos );
        float density = volume_data.Sample( nearest_sampler, float3(0, 0, 0) ).r;

        if ( density > 0.0f ) {
            return 1.0f;
        }

        accumulated += density * 200.0f;
        t += step_size;

        if ( t > t_max ) {
            break;
        }
    }

    return accumulated;
}

[shader( "fragment" )]
float4 fs_main( VertexOutput in )
    : SV_Target
{
    float3 world_pos = in.position;
    float3 camera_pos = camera_buffer_data.camera_pos;

    // Direction vectors
    float3 normal = in.normal;
    float3 light = normalize( float3( 1, 1, 1 ) );
    float3 view = normalize( world_pos - camera_pos );

    float output_density = raymarch( view, world_pos );

    float3 output = output_density;

    return float4( in.uv, 0.0, 1.0 );
}