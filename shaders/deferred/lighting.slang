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
layout( binding = 3, set = 2 ) Texture2D<float4> octahedral_sky_mips;

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

#if RACECAR_RAY_TRACING
layout( binding = 0, set = 5 ) RaytracingAccelerationStructure sceneBVH;
layout( binding = 0, set = 6 ) Texture2D<float4> ReflectionData;
// R: hit distance, negative on miss
layout( binding = 1, set = 6 ) Texture2D<float4> ReflectionRayData;
#endif // RACECAR_RAY_TRACING

// #include "../utils.slang"
// #include "../glint/glint_noise_init.hlsl"
// #include "../glint/glint.hlsl"

#include "../car_mat/car_lighting.slang"

#if RACECAR_RAY_TRACING
// match src/passes/reflection_mip_chain.hpp
static const float REFLECTION_MAX_MIP = 5.0f;

float4 cubic_bspline_weights( float v )
{
    float4 n = float4( 1.0f, 2.0f, 3.0f, 4.0f ) - v;
    float4 s = n * n * n;

    float x = s.x;
    float y = s.y - 4.0f * s.x;
    float z = s.z - 4.0f * s.y + 6.0f * s.x;
    float w = 6.0f - x - y - z;

    return float4( x, y, z, w ) / 6.0f;
}

float4 sample_bicubic( Texture2D<float4> tex, float2 uv, float lod )
{
    uint base_width;
    uint base_height;
    tex.GetDimensions( base_width, base_height );

    float2 size = max( floor( float2( float( base_width ), float( base_height ) ) / exp2( lod ) ),
        float2( 1.0f ) );

    float2 coord = uv * size - 0.5f;
    float2 f = frac( coord );
    coord -= f;

    float4 wx = cubic_bspline_weights( f.x );
    float4 wy = cubic_bspline_weights( f.y );

    float2 sx = float2( wx.x + wx.y, wx.z + wx.w );
    float2 sy = float2( wy.x + wy.y, wy.z + wy.w );

    float2 ox = ( float2( coord.x - 0.5f, coord.x + 1.5f ) + float2( wx.y, wx.w ) / sx ) / size.x;
    float2 oy = ( float2( coord.y - 0.5f, coord.y + 1.5f ) + float2( wy.y, wy.w ) / sy ) / size.y;

    float4 s00 = tex.SampleLevel( nearest_sampler, float2( ox.x, oy.x ), lod );
    float4 s10 = tex.SampleLevel( nearest_sampler, float2( ox.y, oy.x ), lod );
    float4 s01 = tex.SampleLevel( nearest_sampler, float2( ox.x, oy.y ), lod );
    float4 s11 = tex.SampleLevel( nearest_sampler, float2( ox.y, oy.y ), lod );

    float tx = sx.x / ( sx.x + sx.y );
    float ty = sy.x / ( sy.x + sy.y );

    return lerp( lerp( s11, s01, tx ), lerp( s10, s00, tx ), ty );
}

float4 sample_reflection( float2 uv, float lod )
{
    float lod0 = floor( lod );
    float lod1 = min( lod0 + 1.0f, REFLECTION_MAX_MIP );
    float lod_t = lod - lod0;

    float4 refl0 = sample_bicubic( ReflectionData, uv, lod0 );
    float4 refl1 = sample_bicubic( ReflectionData, uv, lod1 );

    float coverage0 = sample_bicubic( ReflectionRayData, uv, lod0 ).b;
    float coverage1 = sample_bicubic( ReflectionRayData, uv, lod1 ).b;

    return lerp( refl0, refl1, lod_t ) * lerp( coverage0, coverage1, lod_t );
}

static const float REFLECTION_MISS_DISTANCE = 1e4f;

float reflection_hit_distance( float2 uv )
{
    float hit_distance = ReflectionRayData.SampleLevel( point_sampler, uv, 0.0f ).r;

    return hit_distance < 0.0f ? REFLECTION_MISS_DISTANCE : hit_distance;
}

float reflection_lod( float roughness, float hit_distance, float view_distance, float screen_height )
{
    float alpha = roughness * roughness;
    float cone_radius = hit_distance * alpha;

    float fov_y = camera_buffer_data.camera_constants.w;
    float proj_scale = 0.5f * screen_height / tan( 0.5f * fov_y );

    float footprint = cone_radius * proj_scale / max( view_distance + hit_distance, 0.001f );

    return clamp( log2( max( footprint, 1.0f ) ), 0.0f, REFLECTION_MAX_MIP );
}

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

    float4 refl = float4( 0.0f );
    float4 refl_cc = float4( 0.0f );

#if RACECAR_RAY_TRACING
    {
        uint refl_width;
        uint refl_height;
        ReflectionData.GetDimensions( refl_width, refl_height );

        float hit_distance = reflection_hit_distance( in.uv );
        float view_distance = length( camera_pos - in_pos );

        refl = sample_reflection(
            in.uv,
            reflection_lod( roughness, hit_distance, view_distance, float( refl_height ) )
        );
        refl_cc = sample_reflection(
            in.uv,
            reflection_lod( pbr.clearcoat_roughness, hit_distance, view_distance,
                float( refl_height ) )
        );
    }
#endif // RACECAR_RAY_TRACING

    GBuffer gbuffer = { in_pos, normal, in_tangent, in_uv, ddx_uv, ddy_uv };

    bool hit = false;

#if RACECAR_RAY_TRACING
    if ( debug_data.ray_traced_shadows ) {
        float t;
        int primitive_index;
        hit = traceRayFirstHit(
            sceneBVH, in_pos + 0.01 * normalize( in_nor ), light, t, primitive_index );
    }
#endif // RACECAR_RAY_TRACING

    float3 out_color
        = car_bsdf(
            base_albedo, normal, view, half, light, pbr, gbuffer, hit ? 0.0 : 1.0, refl, refl_cc );

    // DEBUG VIEWS
    switch ( debug_data.debug_view ) {
    case DEBUG_VIEW_NORMALS:
        return float4( normal, 1.0f );
    case DEBUG_VIEW_ALBEDO:
        return float4( base_albedo, 1.0f );
    case DEBUG_VIEW_ROUGHNESS_METAL:
        return float4( roughness, metallic, 0.0f, 1.0f );
    default:
        break;
    }

    return float4( out_color, 1.0f );
}


