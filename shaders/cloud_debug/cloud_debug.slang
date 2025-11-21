struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT0;
    float2 uv : TEXCOORD0;
};

struct VertexOutput {
    float4 sv_position : SV_POSITION;
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

struct DebugData {
    bool enable_albedo_map;
    bool enable_normal_map;
    bool enable_roughness_metal_map;
    bool normals_only;
    bool albedo_only;
    bool roughness_metal_only;
}

// State for PCG random number generation
struct RngState {
    uint32_t state;
}

layout( binding = 0, set = 0 ) ConstantBuffer<CameraBufferData> camera_buffer_data;
layout( binding = 1, set = 0 ) ConstantBuffer<DebugData> debug_data;

layout( binding = 0, set = 1 ) Texture3D<float4> volume_data;

layout( binding = 0, set = 2 ) SamplerState nearest_sampler;

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

uint32_t wang32( uint32_t x )
{
    x = ( x ^ 61u ) ^ ( x >> 16 );
    x *= 9u;
    x ^= x >> 4;
    x *= 0x27d4eb2du;
    x ^= x >> 15;
    return x;
}

// Returns in range [0...1)
float rand_float( float seed )
{
    // Mask bottom 24-bits
    return ( wang32( asuint( seed ) ) & 0x00FFFFFFu ) / 16777216.0;
}

float rand_float2( float2 p )
{
    uint32_t seed = ( asuint( p.x ) ) * 73856093u ^ ( asuint( p.y ) ) * 19349663u;
    return rand_float( seed );
}

float rand_float3( float3 p )
{
    uint32_t seed = ( asuint( p.x ) ) * 73856093u ^ ( asuint( p.y ) ) * 19349663u
        ^ ( asuint( p.z ) ) * 83492791u;
    return rand_float( seed );
}

float3 rand_float3_float3( float3 p )
{
    uint32_t seed = ( asuint( p.x ) ) * 73856093u ^ ( asuint( p.y ) ) * 19349663u
        ^ ( asuint( p.z ) ) * 83492791u;
    float x = rand_float3( seed );
    float y = rand_float( x );
    float z = rand_float( y );
    return normalize( 2.f * float3( x, y, z ) - float3( 1.f ) );
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
    t_max = min( t_far.x, min( t_far.y, t_far.z ) );

    return max( 0.0f, t_min ) <= t_max;
}

float sample_density( float3 march_pos )
{
    float3 p = march_pos * 1.f + float3( 1000.f, 0.f, 1100.f );

    float cumulus_map = noise_sum3D( float3( p.xz * 2.f + 100.f, 1.f ) );
    cumulus_map += 0.5;
    cumulus_map = smoothstep( 0.0, 1.0, cumulus_map );

    float normY = -p.y * 0.5 + 0.5;
    float height_modulation = ( 0.5 * ( -normY + 1 ) ) / ( cumulus_map - 0.2 );
    float value2 = ( 0.5 * ( 10 * normY - 8 ) ) / ( cumulus_map - 0.2 );
    height_modulation = smoothstep( 0.6, 0.4, height_modulation ) * smoothstep( 1.0, 0.0, value2 );

    float worley = noise_sum3D_Worley( p.xyz * 1.5 + 100.f );
    worley = smoothstep( 0.0, 0.8, worley ) - 0.2;

    float low_freq_perlin = noise_sum3D( p.xyz * 1.5 + 100.f ) + 0.5;
    low_freq_perlin = 0.8 * smoothstep( 0.3, 0.6, low_freq_perlin ) + 0.2;
    float low_freq_noise = low_freq_perlin * ( 1.2 - worley ) * 0.5;
    low_freq_noise *= 0.5;
    low_freq_noise -= 0.25;

    float low_freq_modulation = 1.f - height_modulation;

    cumulus_map = lerp( cumulus_map, cumulus_map + 4 * low_freq_noise, low_freq_modulation );
    cumulus_map = smoothstep( 0.3, 0.6, cumulus_map ) * 2 - 1;

    return 0.5f * ( max( cumulus_map * height_modulation, 0 ) );
}

static const float3 light_dir = normalize( float3( 1, 1, 1 ) );

static const float absorption_coeff = 11.5f;

float estimate_lighting( float3 march_pos )
{
    float3 local_light_dir = normalize( mul( (float3x3)camera_buffer_data.inv_model, light_dir ) );

    float3 minAABB = float3( -1, -1, -1 );
    float3 maxAABB = float3( 1, 1, 1 );

    float t_min, t_max;
    if ( !check_ray_bbox_intersection(
             march_pos, local_light_dir, minAABB, maxAABB, t_min, t_max ) ) {
        return 1.0f;
    }

    float3 right_dir;
    if ( dot( local_light_dir, float3( 0, 1, 0 ) ) > 0.01 ) {
        right_dir = cross( local_light_dir, float3( 0, 1, 0 ) );
    } else {
        right_dir = cross( local_light_dir, float3( 1, 0, 0 ) );
    }

    float3 up_dir = cross( local_light_dir, right_dir );

    // Cone sample
    float step_size = 0.01;
    float throughput = 1.f;
    for ( uint i = 0; i < 5; i++ ) {
        float3 prev_marched_pos = march_pos;
        step_size += step_size * 0.1 * ( rand_float( prev_marched_pos.x ) * 0.5 - 0.25 );

        float2 coneOffset = rand_float3_float3( march_pos ).xy;
        float dist_along_view = i * step_size;
        float3 marched_pos = march_pos + local_light_dir * dist_along_view;
        float coneAngle = 0.3;
        marched_pos += right_dir * coneOffset.x * dist_along_view * coneAngle;
        marched_pos += up_dir * coneOffset.y * dist_along_view * coneAngle;

        float density = sample_density( march_pos );
        throughput
            *= exp( -absorption_coeff * density * distance( marched_pos, prev_marched_pos ) );
        step_size = 0.05;
    }

    float sun_amplitude = 1.0f;
    return throughput * sun_amplitude;
}

float PhaseHG( float cosTheta, float g )
{
    float g2 = g * g;
    float denom = pow( 1 + g2 - 2 * g * cosTheta, 1.5 );
    return ( 1 - g2 ) / ( 4 * 3.14159265 * denom );
}

float3 raymarch( float3 view, float3 origin )
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

    int steps = 32;
    float t = 0.0f;
    float step_size = ( t_max - t_min ) / (float)steps;
    float3 accumulated = float3( 0.0f );

    float throughput = 1.f;
    for ( float t = t_min; t < t_max; t += step_size ) {
        if ( throughput < 0.01 ) {
            break;
        }
        // jitter
        t += step_size * 0.1 * ( rand_float( t * origin.x ) * 0.5 - 0.25 );
        float3 marched_pos = local_origin + local_dir * t;
        float density = sample_density( marched_pos );
        if ( density > 0.01 ) {

            float lighting = estimate_lighting( marched_pos );

            float3 local_light_dir
                = normalize( mul( (float3x3)camera_buffer_data.inv_model, light_dir ) );

            float cosTheta = dot( local_dir, local_light_dir );
            float phase = PhaseHG( cosTheta, 0.947 );

            float scattering = density * lighting * 1.f;
            accumulated += float3( throughput * scattering );
            throughput *= exp( -density * step_size * absorption_coeff );
        }
    }

    return accumulated * float3( 1.0, 1.0, 0.8 );
}

float worley3D( float3 p )
{
    float r = 3.0;
    float3 f = floor( p );
    float3 x = fract( p );
    for ( int i = -1; i <= 1; i++ ) {
        for ( int j = -1; j <= 1; j++ ) {
            for ( int k = -1; k <= 1; k++ ) {
                float3 q = float3( float( i ), float( j ), float( k ) );
                float3 v = q
                    + float3( rand_float3( ( q + f ) * 1.11 ), rand_float3( ( q + f ) * 1.14 ),
                        rand_float3( ( q + f ) * 1.17 ) )
                    - x;
                float d = dot( v, v );
                r = min( r, d );
            }
        }
    }
    return sqrt( r );
}

float perlin_noise3D( float3 p )
{
    float3 grid_cell = floor( p );
    float3 fractional = p - grid_cell;

    float3 w = fractional * fractional * ( 3. - 2. * fractional );

    float f000 = dot(
        rand_float3_float3( grid_cell + float3( .0, .0, 0. ) ), fractional - float3( .0, .0, 0. ) );
    float f010 = dot(
        rand_float3_float3( grid_cell + float3( .0, 1., 0. ) ), fractional - float3( .0, 1., 0. ) );
    float f100 = dot(
        rand_float3_float3( grid_cell + float3( 1., 0., 0. ) ), fractional - float3( 1., 0., 0. ) );
    float f110 = dot(
        rand_float3_float3( grid_cell + float3( 1., 1., 0. ) ), fractional - float3( 1., 1., 0. ) );
    float f001 = dot(
        rand_float3_float3( grid_cell + float3( .0, .0, 1. ) ), fractional - float3( .0, .0, 1. ) );
    float f011 = dot(
        rand_float3_float3( grid_cell + float3( .0, 1., 1. ) ), fractional - float3( .0, 1., 1. ) );
    float f101 = dot(
        rand_float3_float3( grid_cell + float3( 1., 0., 1. ) ), fractional - float3( 1., 0., 1. ) );
    float f111 = dot(
        rand_float3_float3( grid_cell + float3( 1., 1., 1. ) ), fractional - float3( 1., 1., 1. ) );

    float f00 = lerp( f000, f001, w.z );
    float f01 = lerp( f010, f011, w.z );
    float f10 = lerp( f100, f101, w.z );
    float f11 = lerp( f110, f111, w.z );

    float xm1 = lerp( f00, f10, w.x );
    float xm2 = lerp( f01, f11, w.x );

    float ym = lerp( xm1, xm2, w.y );
    return ym;
}

// fractal perlin noise
float noise_sum3D( float3 p )
{
    p *= 1.; // Cell size
    // amplitude, noise_sum, total_sum
    float a = 1., r = 0., s = 0.;

    for ( int i = 0; i < 5; i++ ) {
        r += a * perlin_noise3D( p );
        s += a;
        p *= 2.;
        a *= .5;
    }

    return r / s;
}

// fractal worley noise
float noise_sum3D_Worley( float3 p )
{
    p *= 4.; // Cell size
    // amplitude, noise_sum, total_sum
    float a = 1., r = 0., s = 0.;

    for ( int i = 0; i < 5; i++ ) {
        r += a * worley3D( p );
        s += a;
        p *= 2.;
        a *= .5;
    }

    return r / s;
}

[shader( "fragment" )]
float4 fs_main( VertexOutput in )
    : SV_Target
{
    float3 world_pos = in.position;
    float3 camera_pos = camera_buffer_data.camera_pos;

    float3 normal = in.normal;
    float3 light = normalize( float3( 1, 1, 1 ) );
    float3 view = normalize( world_pos - camera_pos );

    // float output_density = raymarch( view, world_pos );

    float3 uv = in.sv_position.xyy / float3( 1280, 1280, 1280 );

    float cumulus_map = noise_sum3D( uv * 6 + 100.f );
    cumulus_map += 0.5;
    cumulus_map = smoothstep( 0.6, 1.2, cumulus_map );

    float worley = noise_sum3D_Worley( uv * 7 + 100.f );
    worley = smoothstep( 0.0, 0.8, worley ) - 0.2;

    float low_freq_perlin = noise_sum3D( uv * 7 + 100.f ) + 0.5;
    low_freq_perlin = 0.8 * smoothstep( 0.3, 0.6, low_freq_perlin ) + 0.2;
    float low_freq_noise = low_freq_perlin * ( 1.2 - worley ) * 0.5;
    low_freq_noise *= 0.5;

    float high_freq_noise = noise_sum3D_Worley( uv * 7 + 100.f );
    high_freq_noise = 0.8 - 0.7 * smoothstep( 0.0, 0.8, high_freq_noise );

    // return float4( float3( high_freq_noise ), 1.0 );
    float2 coords = fract( 2 * uv.xy );
    float height_modulation = ( 0.5 * ( -coords.y + 1 ) ) / coords.x;
    float value2 = ( 0.5 * ( 10 * coords.y - 8 ) ) / coords.x;
    height_modulation = smoothstep( 0.6, 0.4, height_modulation ) * smoothstep( 1.0, 0.0, value2 );

    float3 output_density = raymarch( view, world_pos );

    return float4( output_density, 1.0 );
}


