layout( binding = 0, set = 0 ) public TextureCube<float4> cubemap;
layout( binding = 1, set = 0 ) public RWTexture2DArray<float4> irradiance_cubemap;

layout( binding = 0, set = 1 ) public SamplerState nearest_sampler;

layout( binding = 0, set = 2 ) public RWTexture2D<float4> sh_coefficients;

public static const float PI = 3.14159265358979323846f;

float4 get_cube_direction( uint2 pixel, uint face, float face_size ) {
    float px = (float) pixel.x;
    float py = (float) pixel.y;

    float u = 2.0f * ( ( px + 0.5f ) / face_size ) - 1.0f;
    float v = 2.0f * ( ( py + 0.5f ) / face_size ) - 1.0f;

    // should we flip v?? idk broski
    v = -v;
    float3 dir;

    switch ( face )
    {
    case 0:
        dir = float3( 1.0f, v, -u);
        break;
    case 1:
        dir = float3( -1.0f, v, u );
        break;
    case 2:
        dir = float3( u, 1.0f, -v );
        break;
    case 3:
        dir = float3( u, -1.0f, v );
        break;
    case 4:
        dir = float3( u, v, 1.0f );
        break;
    case 5:
        dir = float3( -u, v, -1.0f );
        break;
    }

    // float inv_denom = 1.0f / pow( 1.0f + u * u + v * v, 1.5f );
    // float solid_angle = inv_denom * ( 4.0f / ( face_size * face_size ) );

    float solid_angle = solid_angle_exact( u, v, face_size );

    return float4(normalize(dir), solid_angle);
}


[shader("compute")]
[numthreads(1, 1, 1)]
void cs_compute_irradiance( uint3 thread_id: SV_DispatchThreadID ) {
    uint2 xy = thread_id.xy;
    uint face = thread_id.z;

    float hardcoded_face_size = 256.0f;

    float3 normal = get_cube_direction( xy, face, hardcoded_face_size ).rgb;
    float3 up = float3(0.0f, 1.0f, 0.0f);
    float3 right = normalize( cross( up, normal ) );
    up = normalize( cross( normal, right ) );

    float3 irradiance = float3(0.0f);
    float sample_delta = 0.025f;
    float number_samples = 0.0f;

    for ( float phi = 0.0f; phi < 2.0f * PI; phi += sample_delta ) {
        for ( float theta = 0.0f; theta < 0.5f * PI; theta += sample_delta ) {
            float3 tangent_sample = float3( sin( theta ) * cos( phi ), sin( theta ) * sin( phi ), cos( theta ) );
            float3 sample_dir = normalize(tangent_sample.x * right + tangent_sample.y * up + tangent_sample.z * normal);

            irradiance += cubemap.SampleLevel( nearest_sampler, sample_dir, 0 ).rgb * cos( theta ) * sin( theta );
            number_samples += 1.0f;
        }
    }

    irradiance = PI * irradiance * (1.0f / number_samples);

    float3 colors[6] = {
        float3( 1.0f, 0.0f, 0.0f ),
        float3( 0.0f, 1.0f, 0.0f ),
        float3( 0.0f, 0.0f, 1.0f ),
        float3( 1.0f, 1.0f, 0.0f ),
        float3( 0.0f, 1.0f, 1.0f ),
        float3( 1.0f, 1.0f, 1.0f )
    };

    irradiance_cubemap[uint3( xy, face )] = float4( irradiance, 0.0f );
}

float eval_SH( uint lm_index, float3 direction )
{
    if ( lm_index == 0 ) // 0,  0
        return 0.282095f;
    if ( lm_index == 1 ) // 1, -1
        return 0.488603f * direction.y;
    if ( lm_index == 2 ) // 1,  0
        return 0.488603f * direction.z;
    if ( lm_index == 3 ) // 1,  1
        return 0.488603f * direction.x;
    if ( lm_index == 4 ) // 2, -2
        return 1.092548f * ( direction.x * direction.y );
    if ( lm_index == 5 ) // 2, -1
        return 1.092548f * ( direction.y * direction.z );
    if ( lm_index == 6 ) // 2,  0
        return 0.315392f * ( 3.0f * ( direction.z * direction.z ) - 1.0f );
    if ( lm_index == 7 ) // 2,  1
        return 1.092548f * ( direction.x * direction.z );
    if ( lm_index == 8 ) // 2,  2
        return 0.546274f * ( ( direction.x * direction.x ) - ( direction.y * direction.y ) );

    return -1.0f;
}

float area_element( float x, float y )
{
    return atan2( x * y, sqrt( x * x + y * y + 1.0 ) );
}

float solid_angle_exact( float u, float v, float face_size )
{
    float inv = 1.0 / face_size;
    float x0 = u - inv;
    float y0 = v - inv;
    float x1 = u + inv;
    float y1 = v + inv;

    return area_element( x0, y0 )
         - area_element( x0, y1 )
         - area_element( x1, y0 )
        + area_element( x1, y1 );
}

groupshared float3 sh_tmp[9][6];

[shader("compute")]
[numthreads( 9, 6, 1 )]
void cs_compute_irradiance_sh( uint3 dispatchThreadID: SV_DispatchThreadID,
    uint3 groupThreadID: SV_GroupThreadID,
                              uint3 groupID: SV_GroupID )
{
    // thread index within the group
    uint lx = groupThreadID.x; // 0..8
    uint ly = groupThreadID.y; // 0..5
    uint coeffIndex = lx; // which of the 9 coefficients this thread handles

    float hardcoded_face_size = 256.0f;

    float3 sh_coeffs_local = float3( 0.0f, 0.0f, 0.0f );

    // iterate texels; ensure same mapping as CPU (center sampling)
    for ( uint i = 0; i < 256; ++i ) {
        for ( uint j = 0; j < 256; ++j ) {
            float4 dirAndWeight = get_cube_direction( uint2( i, j ), ly, hardcoded_face_size );
            float3 dir = dirAndWeight.xyz;
            float texelSolidAngle = dirAndWeight.w; // ensure get_cube_direction returns same solid angle as CPU

            float3 color = cubemap.SampleLevel( nearest_sampler, dir, 0 ).rgb;

            // COLOR SPACE: only linearize if CPU did NOT already give linear
            // If your texture is sRGB and hardware doesn't linearize, you'd want: color = pow(color, 2.2f);
            // If your CPU loader returned linear floats, do NOT pow here.

            sh_coeffs_local += color * eval_SH( coeffIndex, dir ) * texelSolidAngle;
        }
    }

    // store per-face per-coeff in groupshared (index using groupThreadID)
    sh_tmp[lx][ly] = sh_coeffs_local;

    GroupMemoryBarrierWithGroupSync();

    // One thread per coefficient reduces across faces
    if ( ly == 0 ) {
        float3 sh_accumulated = float3( 0.0f );
        for ( uint face = 0; face < 6; ++face ) {
            sh_accumulated += sh_tmp[lx][face];
        }

        // Normalize same as CPU: divide by surface area 4*pi
        float3 sh_final = sh_accumulated * ( 1.0f / ( 4.0f * 3.14159265358979323846f ) );

        if ( lx == 0 ) {
            sh_final *= 3.141593;
        }
        if ( lx >= 1 && lx <= 3 ) {
            sh_final *= 2.094395;
        }
        if ( lx >= 4 && lx <= 8 ) {
            sh_final *= 0.785398;
        }

        // Write normalized coefficient (not the accumulated sum)
        sh_coefficients[uint2( lx, 0 )] = float4( sh_final, 1.0f );
    }

    return;
}

