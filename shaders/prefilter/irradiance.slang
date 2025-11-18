layout( binding = 0, set = 0 ) public TextureCube<float4> cubemap;
layout( binding = 1, set = 0 ) public RWTexture2DArray<float4> irradiance_cubemap;
layout( binding = 2, set = 0 ) public SamplerState nearest_sampler;

public static const float PI = 3.14159265358979323846f;

float3 get_cube_direction( uint2 pixel, uint face, float face_size ) {
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

    return dir;
}


[shader("compute")]
[numthreads(1, 1, 1)]
void cs_compute_irradiance( uint3 thread_id: SV_DispatchThreadID ) {
    uint2 xy = thread_id.xy;
    uint face = thread_id.z;

    float hardcoded_face_size = 256;

    float3 normal = get_cube_direction( xy, face, hardcoded_face_size );
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

[shader("compute")]
[numthreads( 8, 8, 8 )]
void cs_compute_irradiance_sh() {
    
    

}