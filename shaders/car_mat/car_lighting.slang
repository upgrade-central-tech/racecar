// module car_lighting;

#include "../utils.slang"
#include "../glint/glint.hlsl"

#define USE_GLINTS 1
#define USE_SH_IRRADIANCE 1

float3 car_bsdf( float3 albedo, float3 normal, float3 view, float3 half, float3 light, Material pbr,
    GBuffer g_buffer )
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
    float roughness =  pbr.roughness;
    float clearcoat_weight = pbr.clearcoat_weight;
    float clearcoat_roughness = pbr.clearcoat_roughness;
    float2 uv = g_buffer.in_uv;

    ////////////////////////////////////////////////////////////////
    // Specular reflection
    ////////////////////////////////////////////////////////////////
#if 1
    float3 base_albedo = lerp( float3( 0.3, 0.1, 0.82 ), float3( 0.8, 0.3, 0.5 ), pow( clamped_n_dot_h, 5.0f ) );
#endif

    // So far only the first mip exists. Will need some more complicated efforts to
    // get this stuff to work with roughness.

    // write out ugly ass code here first, then make it nicer later
    float2 encoded_direction = oct_encode( reflect( -view, normal ) );

    // float3 sky_color = octahedral_sky.Sample( nearest_sampler, encoded_direction ).rgb;

    float3 precomputed_lo_specular = octahedral_sky.Sample( nearest_sampler, encoded_direction ).rgb;

    float3 F0 = float3( 0.04f );
    F0 = lerp( F0, base_albedo * precomputed_lo_specular, pbr.metallic );

    // Base specular
    float alpha = pbr.roughness * pbr.roughness;
    float NDF = DistributionGGX( clamped_n_dot_h, alpha );
    float G = GeometrySmith( clamped_n_dot_v, clamped_n_dot_l, alpha );
    float3 F = FresnelSchlick( clamped_n_dot_h, F0 );

    float2 brdf_uv = float2( clamped_n_dot_v, pbr.roughness );
    float2 brdf = BRDF_LUT.Sample( nearest_sampler, brdf_uv ).rg;

    // Point-light solution is found via:
#if USE_GLINTS
    float3 local_half = world_to_local( half, normal );
    float2 duvdx = g_buffer.ddx_uv;
    float2 duvdy = g_buffer.ddy_uv;

    float maxNDF = 10000.0f;
    float targetNDF = NDF;
    NDF = duvdy.x; // SampleGlints2023NDF( local_half, targetNDF, maxNDF, uv, duvdx, duvdy ).x;
#endif

    float3 base_specular = ( NDF * G * F ) / ( 4.0f * clamped_n_dot_v * clamped_n_dot_l + 0.001f );
    // We use Split-sum approximation instead:
    // float3 base_specular = precomputed_lo_specular * ( F0 * brdf.x + brdf.y );

    // Clearcoat evaluation
    float alpha_cc = clearcoat_roughness * clearcoat_roughness;
    float D_cc = DistributionGTR( lerp( 0.1f, 0.001f, alpha_cc ), abs( n_dot_h ) );
    float G_cc = GeometrySmith( clamped_n_dot_v, clamped_n_dot_l, alpha_cc );
    float3 F_cc = FresnelSchlick( clamped_v_dot_h, float3( 0.04 ) );
    float3 specular_cc
        = ( F_cc * D_cc * G_cc ) / ( 4.0f * clamped_n_dot_v * clamped_n_dot_l + 0.001f );

    // brdf_cc needs to be prefiltered using the GTR1 (Generalized Trowbridge Reitz) NDF
    float3 F0_cc = float3( 0.04f );
    float2 brdf_cc = brdf;
    float3 precomputed_specular_cc = precomputed_lo_specular * ( F0_cc * brdf.x + brdf.y );

    float3 clearcoat_specular = clearcoat_weight * 1.0f * precomputed_specular_cc;
    float3 specular_final = max( clearcoat_specular + ( 1.0f - F_cc ) * base_specular, 0.0f );

    ////////////////////////////////////////////////////////////////
    // Diffuse reflection
    // Based on Disney 2012 diffuse model:
    // https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf
    ////////////////////////////////////////////////////////////////
    float3 diffuse_color = lerp( albedo, float3( 0.8, 0.3, 0.5 ), pow( clamped_n_dot_h, 5.0f ) );
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
    float3 base_color = ( diffuse_final + max( ( 1.0f - F_cc ) * base_specular, 0.0f ) );
    // float3 out_color = base_color + clearcoat_specular;

#if USE_SH_IRRADIANCE
    float3 irradiance = eval_SH( sh_data, normal );
    float3 diffuse_irradiance = diffuse_final * irradiance;
    float3 diffuse_irradiance2 = irradiance_cubemap.Sample( nearest_sampler, normal ).rgb;
    // out_color *= irradiance;
#endif

    float3 diffuse_direct = diffuse_final * clamped_n_dot_l;

    float3 specular_direct = base_specular * clamped_n_dot_l;
    float3 specular_indirect = precomputed_specular_cc;

    float3 clearcoat_ibl = clearcoat_weight * 1.0f * precomputed_specular_cc;
    float3 clearcoat_direct = max( clearcoat_specular + ( 1.0f - F_cc ) * base_specular, 0.0f );

    float3 kS = F;
    float3 kD = 1.0f - kS;
    kD *= 1.0f - metallic;

    float3 out_color = float3( 0.0f );

    // Direct lighting
    out_color += ( diffuse_direct + specular_direct );

    // IBL
    // Removed kD term due to strange shadows
    out_color += ( diffuse_irradiance + specular_indirect );
    out_color += clearcoat_ibl;

    return out_color;
}


