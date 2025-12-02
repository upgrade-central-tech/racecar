// module car_lighting;

#include "../utils.slang"
#include "../sh_utils.slang"
#include "../glint/glint.hlsl"

#define USE_GLINTS 1
#define USE_SH_IRRADIANCE 0

float3 sample_glossy_ibl( float roughness, float2 direction )
{
    float lod = roughness * 5.0f;
    uint lod0 = uint( floor( roughness * 5.0f ) );
    uint lod1 = min( lod0 + 1, 5 );
    float lod_t = lod - lod0;

    float3 glossy_lod0 = octahedral_sky_mips.SampleLevel( nearest_sampler, direction, lod0 ).rgb;
    float3 glossy_lod1 = octahedral_sky_mips.SampleLevel( nearest_sampler, direction, lod1 ).rgb;
    return lerp( glossy_lod0, glossy_lod1, lod_t );
}

float3 car_bsdf( float3 albedo, float3 normal, float3 view, float3 half, float3 light, Material pbr,
    GBuffer g_buffer, float sun_V )
{
    // Essential dot products
    float n_dot_v = ( dot( normal, view ) );
    float n_dot_h = ( dot( normal, half ) );
    float n_dot_l = ( dot( normal, light ) );
    float h_dot_l = ( dot( half, light ) );
    float v_dot_h = ( dot( view, half ) );

    float clamped_n_dot_v = max( n_dot_v, 0.0f );
    float clamped_n_dot_h = max( n_dot_h, 0.0f );
    float clamped_n_dot_l = max( n_dot_l, 0.0f );
    float clamped_h_dot_l = max( h_dot_l, 0.0f );
    float clamped_v_dot_h = max( v_dot_h, 0.0f );

    // Struct props
    float metallic = pbr.metallic;
    float roughness = pbr.roughness;
    float clearcoat_weight = pbr.clearcoat_weight;
    float clearcoat_roughness = pbr.clearcoat_roughness;
    float2 uv = g_buffer.in_uv;

    ////////////////////////////////////////////////////////////////
    // Specular reflection
    ////////////////////////////////////////////////////////////////
#if 1
    float3 base_albedo = albedo; // lerp( float3( 0.3, 0.1, 0.82 ), float3( 0.8, 0.3, 0.5 ), pow(
                                 // clamped_n_dot_h, 5.0f ) );
#endif

    // So far only the first mip exists. Will need some more complicated efforts to
    // get this stuff to work with roughness.

    float2 encoded_direction = oct_encode( reflect( -view, normal ) );
    float3 precomputed_lo_specular = sample_glossy_ibl( roughness, encoded_direction );

    float3 F0 = float3( 0.04f );
    F0 = lerp( F0, base_albedo, pbr.metallic );
    float3 F_indirect = FresnelSchlickRoughness( clamped_n_dot_v, F0, pbr.roughness);

    // Base specular
    float alpha = pbr.roughness * pbr.roughness;
    float NDF = DistributionGGX( clamped_n_dot_h, alpha );
    float G = GeometrySmith( clamped_n_dot_v, clamped_n_dot_l, alpha );
    float3 F = FresnelSchlick( clamped_n_dot_h, F0 );

    float2 brdf_uv = float2( clamped_n_dot_v, pbr.roughness );
    float2 brdf = BRDF_LUT.Sample( nearest_sampler, brdf_uv ).rg;

    float2 brdf_uv_cc = float2( clamped_n_dot_v, pbr.clearcoat_roughness );
    float2 brdf_cc = BRDF_LUT.Sample( nearest_sampler, brdf_uv_cc );

    // Point-light solution is found via:
#if USE_GLINTS
    float NDF_glints = g_buffer.ddy_uv.x;
    NDF = NDF_glints;
#endif

    float3 glint_specular = ( NDF * G * F ) / ( 4.0f * clamped_n_dot_v * clamped_n_dot_l + 0.001f );

    // Clearcoat evaluation
    // float alpha_cc = clearcoat_roughness * clearcoat_roughness;
    // float D_cc = DistributionGTR( lerp( 0.1f, 0.001f, alpha_cc ), abs( n_dot_h ) );
    // float G_cc = GeometrySmith( clamped_n_dot_v, clamped_n_dot_l, alpha_cc );
    // float3 F_cc = FresnelSchlick( clamped_v_dot_h, float3( 0.04 ) );
    // float3 specular_cc
    //     = ( F_cc * D_cc * G_cc ) / ( 4.0f * clamped_n_dot_v * clamped_n_dot_l + 0.001f );

    // brdf_cc needs to be prefiltered using the GTR1 (Generalized Trowbridge Reitz) NDF
    float3 F0_cc = float3( 0.04f );
    float3 F_cc = FresnelSchlickRoughness( clamped_n_dot_v, F0_cc, clearcoat_roughness);
    float3 precomputed_specular_cc = sample_glossy_ibl( clearcoat_roughness, encoded_direction );
    precomputed_specular_cc *= ( F_cc * brdf_cc.x + brdf_cc.y );

    float3 clearcoat_specular = clearcoat_weight * 1.0f * precomputed_specular_cc;

    ////////////////////////////////////////////////////////////////
    // Diffuse reflection
    // Based on Disney 2012 diffuse model:
    // https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf
    ////////////////////////////////////////////////////////////////
    float3 diffuse_color
        = albedo; // lerp( albedo, float3( 0.8, 0.3, 0.5 ), pow( clamped_n_dot_h, 5.0f ) );
    float3 lambert = diffuse_color / PI;

    // Fresnel weights
    float F_l = pow( 1.0f - clamped_n_dot_l, 5.0f );
    float F_v = pow( 1.0f - clamped_n_dot_v, 5.0f );
    float F_D90 = 0.5f + 2.0f * roughness * clamped_h_dot_l * clamped_h_dot_l;

    float3 diffuse_final
        = lambert; //  * ( 1.0f + ( F_D90 - 1.0f ) * F_l ) * ( 1.0f + ( F_D90 - 1.0f ) * F_v );

    ////////////////////////////////////////////////////////////////
    // Composite results
    ////////////////////////////////////////////////////////////////

#if USE_SH_IRRADIANCE
    // float3 irradiance = eval_SH( sh_data, normal );
    // float3 diffuse_irradiance = diffuse_final * irradiance;
    // float3 diffuse_irradiance2 = irradiance_cubemap.Sample( nearest_sampler, normal ).rgb;
    //  out_color *= irradiance;
#else
    float3 diffuse_irradiance
        = octahedral_sky_irradiance.Sample( nearest_sampler, encoded_direction ).rgb;
    diffuse_irradiance *= albedo / PI;
#endif
    float3 kD_direct = float3(1.0f) - F;
    kD_direct *= 1.0f - metallic;

    float3 diffuse_direct = diffuse_final;

    float3 specular_direct = glint_specular;
    float3 specular_indirect = precomputed_lo_specular * ( F_indirect * brdf.x + brdf.y );

    float3 clearcoat_indirect = clearcoat_specular;
    // float3 clearcoat_direct
    //     = max( clearcoat_specular + ( 1.0f - F_cc ) * glint_specular, 0.0f ) * sun_V;

    float3 kS = F_indirect;
    float3 kD = 1.0f - kS;
    kD *= 1.0f - metallic;

    float3 out_color = float3( 0.0f );

    // Direct lighting
    out_color += ( kD_direct * diffuse_direct + specular_direct ) * sun_V * clamped_n_dot_l;

    // IBL - Removed kD term due to strange shadows
    out_color += ( kD * diffuse_irradiance + specular_indirect );
    out_color += clearcoat_indirect;

    return out_color;
}


