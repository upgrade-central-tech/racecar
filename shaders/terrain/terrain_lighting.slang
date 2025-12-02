#include "../common.slang"
#include "../utils.slang"
#include "../clouds/cloud_utils.slang"

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_data;
layout( binding = 1, set = 0 ) ConstantBuffer<DebugData> scene_data;

layout( binding = 0, set = 1 ) Texture2D<float4> gbuffer_position;
layout( binding = 1, set = 1 ) Texture2D<float4> gbuffer_normal_ao;
layout( binding = 2, set = 1 ) Texture2D<float4> gbuffer_albedo_roughness;
layout( binding = 3, set = 1 ) RWTexture2D<float4> out_color;

layout( binding = 0, set = 2 ) Texture2D<float4> sky;
layout( binding = 1, set = 2 ) Texture2D<float4> sky_irradiance;
layout( binding = 2, set = 2 ) Texture2D<float4> sky_mips;
layout( binding = 3, set = 2 ) Texture2D<float2> BRDF_LUT;

layout( binding = 0, set = 3 ) SamplerState nearest_sampler;

layout( binding = 0, set = 4 ) RaytracingAccelerationStructure sceneBVH;

// Misc.
float3 direct_lighting( float3 albedo, float3 normal, float roughness, float ao, float3 view )
{
    float3 light = normalize( scene_data.sun_direction.rgb );
    float3 half = normalize( view + light );

    BSDF_Dots dots = get_dots( normal, view, half, light );

    // Diffuse properties
    float3 diffuse_albedo = albedo / PI;

    // Specular properties
    float alpha = roughness * roughness;
    float NDF = DistributionGGX( dots.clamped_n_dot_h, alpha );
    float G = GeometrySmith( dots.clamped_n_dot_v, dots.clamped_n_dot_l, alpha );
    float3 F0 = float3( 0.04f );
    float3 F = FresnelSchlick( dots.clamped_n_dot_h, F0 );

    float3 specular
        = ( NDF * G * F ) / ( 4.0f * dots.clamped_n_dot_v * dots.clamped_n_dot_l + 0.001f );

    // CEDEC 22 GT7 Presentation
    float shadow_strength = 1.0f;
    float local_shadows
        = clamp( dot( normal, light ) - shadow_strength * ( 1.0f - ao ) + 0.5f, 0.0f, 1.0f );

    float clamped_lambert = clamp( dot( normal, light ), 0.0f, 1.0f );

    return ( ( 1.0f - F ) * diffuse_albedo + specular ) * clamped_lambert * local_shadows;
}

float3 sample_glossy_ibl( float roughness, float2 direction )
{
    float lod = roughness * 5.0f;
    uint lod0 = uint( floor( roughness * 5.0f ) );
    uint lod1 = min( lod0 + 1, 5 );
    float lod_t = lod - lod0;

    float3 glossy_lod0 = sky_mips.SampleLevel( nearest_sampler, direction, lod0 ).rgb;
    float3 glossy_lod1 = sky_mips.SampleLevel( nearest_sampler, direction, lod1 ).rgb;
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

    float4 packed_gbuffer_albedo = gbuffer_albedo_roughness[pixel];
    float3 mixed_albedo = packed_gbuffer_albedo.rgb;
    float mixed_roughness = packed_gbuffer_albedo.a;

    float4 packed_gbuffer_normal = gbuffer_normal_ao[pixel];
    float3 mixed_normal = packed_gbuffer_normal.rgb;
    float mixed_ao = packed_gbuffer_normal.a;

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

    float irradiance_strength = 1.0f;
    float3 diffuse_irradiance = filter_float4( sky_irradiance, oct_uv ).rgb;
    // sky_irradiance[irradiance_px].rgb;

    uint specular_width, specular_height;
    sky.GetDimensions( specular_width, specular_height );
    int2 sky_px = int2( oct_refl_uv * float2( specular_width, specular_height ) );
    sky_px = clamp( sky_px, int2( 0, 0 ), int2( specular_width - 1, specular_height - 1 ) );

    float specular_strength = 0.75f;
    float3 specular_irradiance = sample_glossy_ibl( mixed_roughness, oct_refl_uv );

    float metallic = 0.0f;
    float n_dot_v = max( dot( mixed_normal, view ), 0.0f );

    float3 F0 = lerp( float3( 0.04f ), mixed_albedo, 0.04f );
    float2 BRDF = BRDF_LUT.SampleLevel( nearest_sampler, float2( n_dot_v, mixed_roughness ), 0.0f );
    specular_irradiance *= ( F0 * BRDF.x + BRDF.y );

    // sky[sky_px].rgb; // ]filter_float4( sky, oct_refl_uv ).rgb; // [sky_px].rgb;

    //=======================================================================================
    // Final lighting evaluation
    //=======================================================================================

    bool hit = false;
    float3 light = normalize( scene_data.sun_direction.rgb );
    if ( scene_data.ray_traced_shadows ) {
        float t;
        int primitive_index;
        hit = traceRayFirstHit(
            sceneBVH, world_position + 0.01 * normalize( mixed_normal ), light, t, primitive_index );
    }

    float3 final_color;
    if ( !hit ) {
        final_color
            = direct_lighting( mixed_albedo, mixed_normal, mixed_roughness, mixed_ao, view );
    } else {
        final_color = float3( 0.0, 0.0, 0.0 );
    }

    final_color += ( mixed_albedo / PI ) * diffuse_irradiance * irradiance_strength;
    final_color += clamp( specular_irradiance, float3( 0.0f ), float3( 1.0f ) );

    out_color[pixel] = float4( final_color, 1.0f );
}


