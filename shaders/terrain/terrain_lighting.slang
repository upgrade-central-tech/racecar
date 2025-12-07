#include "../common.slang"
#include "../utils.slang"
#include "../clouds/cloud_utils.slang"

import constants;

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_data;
layout( binding = 1, set = 0 ) ConstantBuffer<DebugData> scene_data;
layout( binding = 2, set = 0 ) ConstantBuffer<TerrainData> terrain_data;

layout( binding = 0, set = 1 ) Texture2D<float4> gbuffer_position;
layout( binding = 1, set = 1 ) Texture2D<float4> gbuffer_normal_ao;
layout( binding = 2, set = 1 ) Texture2D<float4> gbuffer_albedo_roughness;
layout( binding = 3, set = 1 ) Texture2D<float4> gbuffer_packed_data;
layout( binding = 4, set = 1 ) RWTexture2D<float4> out_color;

layout( binding = 0, set = 2 ) Texture2D<float4> sky;
layout( binding = 1, set = 2 ) Texture2D<float4> sky_irradiance;
layout( binding = 2, set = 2 ) Texture2D<float4> sky_mips;
layout( binding = 3, set = 2 ) Texture2D<float2> BRDF_LUT;

layout( binding = 0, set = 3 ) SamplerState sampler;

layout( binding = 0, set = 4 ) RaytracingAccelerationStructure sceneBVH;

// Misc.
float3 direct_lighting(
    float3 albedo, float3 normal, float3 light, float NDF, float roughness, float ao, float3 view )
{
    float3 half_vector = normalize( view + light );
    BSDF_Dots dots = get_dots( normal, view, half_vector, light );

    // Diffuse properties
    var diffuse = albedo * INV_PI;

    // Specular properties
    float alpha = roughness * roughness;

    float G = GeometrySmith( dots.clamped_n_dot_v, dots.clamped_n_dot_l, alpha );
    float3 F0 = float3( 0.04f );
    float3 F = FresnelSchlick( dots.clamped_n_dot_h, F0 );

    float3 specular
        = ( NDF * G * F ) / ( 4.0f * dots.clamped_n_dot_v * dots.clamped_n_dot_l + 0.001f );

    float shadow_strength = 1.0f;
    float local_shadows
        = clamp( dot( normal, light ) - shadow_strength * ( 1.0f - ao ) + 0.5f, 0.0f, 1.0f );

    float shadowing = dot( normal, light );

    bool enable_gt7_ao = terrain_data.packed_floats0.x > 0.5f;
    if ( enable_gt7_ao ) {
        // CEDEC 22 GT7 Presentation
        float shadow_strength
            = terrain_data.terrain_data0.x; // terrain_data.gt7_local_shadow_strength;
        float local_shadows
            = clamp( dot( normal, light ) - shadow_strength * ( 1.0f - ao ) + 0.5f, 0.0f, 1.0f );
        shadowing *= local_shadows;
    } else {
        shadowing *= ao;
    }

    // Clamp shadows
    shadowing = clamp( shadowing, 0.0f, 1.0f );

    bool shadowing_only = terrain_data.packed_floats0.y > 0.5f;
    if ( shadowing_only ) {
        return shadowing;
    }

    return ( ( 1.0f - F ) * diffuse + specular ) * shadowing;
}

float3 sample_glossy_ibl( float roughness, float2 direction )
{
    float lod = roughness * 5.0f;
    uint lod0 = uint( floor( roughness * 5.0f ) );
    uint lod1 = min( lod0 + 1, 5 );
    float lod_t = lod - lod0;

    float3 glossy_lod0 = sky_mips.SampleLevel( sampler, direction, lod0 ).rgb;
    float3 glossy_lod1 = sky_mips.SampleLevel( sampler, direction, lod1 ).rgb;
    return lerp( glossy_lod0, glossy_lod1, lod_t );
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

// Main
[shader( "compute" )]
[numthreads( 8, 8, 8 )]
void cs_terrain_draw( uint3 thread_id: SV_DispatchThreadID )
{
    uint2 pixel = thread_id.xy;

    if ( gbuffer_position[pixel].a != TERRAIN_ID ) {
        return;
    }

    float3 world_position = gbuffer_position[pixel].rgb;
    float3 light = normalize( scene_data.sun_direction.rgb );

    float4 packed_gbuffer_albedo = gbuffer_albedo_roughness[pixel];
    float3 mixed_albedo = packed_gbuffer_albedo.rgb;
    float mixed_roughness = packed_gbuffer_albedo.a;

    float4 packed_gbuffer_normal = gbuffer_normal_ao[pixel];
    float3 mixed_normal = packed_gbuffer_normal.rgb;
    float mixed_ao = packed_gbuffer_normal.a;

    float4 packed_gbuffer_data = gbuffer_packed_data[pixel];

    // Misc. vectors
    float3 view = normalize( camera_data.camera_pos.rgb - world_position );

    //=======================================================================================
    // Image-based lighting evaluations
    //=======================================================================================

    // Remap to [0-1]
    float2 oct_uv = oct_encode( mixed_normal );
    float2 oct_refl_uv = oct_encode( normalize( reflect( -view, mixed_normal ) ) );

    // Irradiance lighting type shizz
    uint irradiance_width, irradiance_height;
    sky_irradiance.GetDimensions( irradiance_width, irradiance_height );
    int2 irradiance_px = int2( oct_uv * float2( irradiance_width, irradiance_height ) );
    irradiance_px
        = clamp( irradiance_px, int2( 0, 0 ), int2( irradiance_width - 1, irradiance_height - 1 ) );

    float irradiance_strength = 0.5f;
    float3 diffuse_irradiance = filter_float4( sky_irradiance, oct_uv ).rgb;

    uint specular_width, specular_height;
    sky.GetDimensions( specular_width, specular_height );
    int2 sky_px = int2( oct_refl_uv * float2( specular_width, specular_height ) );
    sky_px = clamp( sky_px, int2( 0, 0 ), int2( specular_width - 1, specular_height - 1 ) );

    float specular_strength = 0.75f;
    float3 specular_irradiance = sample_glossy_ibl( mixed_roughness, oct_refl_uv );

    float metallic = 0.0f;
    float n_dot_v = max( dot( mixed_normal, view ), 0.0f );

    float3 F0 = lerp( float3( 0.04f ), mixed_albedo, 0.04f );
    float2 BRDF = BRDF_LUT.SampleLevel( sampler, float2( n_dot_v, mixed_roughness ), 0.0f );
    specular_irradiance *= ( F0 * BRDF.x + BRDF.y );

    //=======================================================================================
    // Final lighting evaluation
    //=======================================================================================

    bool hit = false;
    if ( scene_data.ray_traced_shadows ) {
        float t;
        int primitive_index;
        hit = traceRayFirstHit( sceneBVH, world_position + 0.01f * normalize( mixed_normal ), light,
            t, primitive_index );
    }

    float3 final_color = float3( 1.f );
    float NDF = packed_gbuffer_data.y;

    if ( !hit ) {
        final_color = direct_lighting(
            mixed_albedo, mixed_normal, light, NDF, mixed_roughness, mixed_ao, view );
    } else {
        final_color = float3( 0.f );
    }

    // // Irradiance run
    final_color += ( mixed_albedo * INV_PI ) * diffuse_irradiance * irradiance_strength;
    final_color += clamp( specular_irradiance, float3( 0.f ), float3( 1.f ) );

    // Debug returns
    bool roughness_only = terrain_data.packed_floats0.z > 0.5f;
    if ( roughness_only ) {
        final_color = float3( mixed_roughness );
    }

    out_color[pixel] = float4( final_color, 1.f );
}


