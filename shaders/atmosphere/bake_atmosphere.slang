#define HARDCODED_OCTAHEDRAL_SIZE 256.0f
#define HARDCODED_IRRADIANCE_SIZE 32.0f
#define COMPUTE_NO_SAMPLE 1

import atmosphere;

// In atmosphere, the bindings are already there and used for other things.
layout( binding = 0, set = 3 ) RWTexture2D<float4> octahedral_sky;
// Octahedral sky used for irradiance
layout( binding = 1, set = 3 ) RWTexture2D<float4> octahedral_sky_irradiance;

float3 oct_decode( float2 f )
{
    // Map from [0, 1] to [-1, 1]
    f = f * 2.0f - 1.0f;

    float3 n = float3( f.x, f.y, 1.0f - abs( f.x ) - abs( f.y ) );

    // Unfold the lower hemisphere
    if ( n.z < 0.0f )
        n.xy = ( 1.0f - abs( n.yx ) ) * sign( n.xy );

    return normalize( n );
}

[shader( "compute" )]
[numthreads( 8, 8, 8 )]
void cs_bake_atmosphere( uint3 thread_id: SV_DispatchThreadID )
{
    // Thread XY represents a direction.
    uint2 pixel = thread_id.xy;
    uint face = thread_id.z;

    float u = ( float( pixel.x ) + 0.5f ) / HARDCODED_OCTAHEDRAL_SIZE;
    float v = ( float( pixel.y ) + 0.5f ) / HARDCODED_OCTAHEDRAL_SIZE;

    float3 view_direction = oct_decode( float2( u, v ) );

    // Insert the work to actually derive a 3D global direction from pixel and face
    float3 sky_color = get_sky_color( view_direction );

    octahedral_sky[pixel] = float4( sky_color, 1.0f );
}

[shader( "compute" )]
[numthreads( 8, 8, 8 )]
void cs_bake_atmosphere_irradiance( uint3 thread_id: SV_DispatchThreadID )
{
    // Thread XY represents a direction.
    uint2 pixel = thread_id.xy;
    uint face = thread_id.z;

    float u = ( float( pixel.x ) + 0.5f ) / HARDCODED_IRRADIANCE_SIZE;
    float v = ( float( pixel.y ) + 0.5f ) / HARDCODED_IRRADIANCE_SIZE;

    float3 view_direction = oct_decode( float2( u, v ) );

    float3 normal = normalize( view_direction );
    float3 up = float3( 0.0f, 1.0f, 0.0f );
    float3 right = normalize( cross( up, normal ) );
    up = normalize( cross( normal, right ) );

    float3 irradiance = float3( 0.0f );
    float sample_delta = 0.1f;
    float number_samples = 0.0f;

    int phi_samples = 8;
    int theta_samples = 8;

    for ( int phi_sample = 0; phi_sample < phi_samples; phi_sample++ ) {
        float phi = ( phi_sample + 0.5f ) * ( 2.0f * PI / phi_samples );

        for ( int theta_sample = 0; theta_sample < theta_samples; theta_sample++ ) {
            float theta = ( theta_sample + 0.5f ) * ( 0.5f * PI / theta_samples );

            float3 tangent_sample
                = float3( sin( theta ) * cos( phi ), sin( theta ) * sin( phi ), cos( theta ) );
            float3 sample_dir = normalize(
                tangent_sample.x * right + tangent_sample.y * up + tangent_sample.z * normal );

            irradiance += get_irradiance_sky_color( sample_dir ) * cos( theta ) * sin( theta );
        }
    }

    irradiance = PI * irradiance * ( 1.0f / ( phi_samples * theta_samples ) );

    octahedral_sky_irradiance[pixel] = float4( irradiance, 1.0f );
}


