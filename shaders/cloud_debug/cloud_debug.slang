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
    float4 camera_pos;
    // Pack: near plane, far plane, aspect ratio, fov_Y
    float4 camera_constants;
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

layout( binding = 0, set = 1 ) Texture3D<float> low_freq_noise_LUT;
layout( binding = 1, set = 1 ) Texture3D<float> high_freq_noise_LUT;
layout( binding = 2, set = 1 ) Texture2D<float> cumulus_map_LUT;

layout( binding = 0, set = 2 ) SamplerState linear_sampler;

float4x4 InversePerspective_DX( float fovY, float aspect, float nearZ, float farZ )
{
    float f = 1.0 / tan( fovY * 0.5 );
    float A = f / aspect;
    float B = f;
    float C = farZ / ( farZ - nearZ );
    float D = ( -nearZ * farZ ) / ( farZ - nearZ );

    return float4x4( 1.0 / A, 0, 0, 0, 0, 1.0 / B, 0, 0, 0, 0, 0, 1.0 / D, 0, 0, 1.0, -C / D );
}

float3 UnprojectNDC_SphericalCamera_ToWorld(
    float3 ndc, float3 camPos, float nearZ, float farZ, float aspect, float fovY )
{
    float4x4 invP = InversePerspective_DX( fovY, aspect, nearZ, farZ );
    float4 viewH = mul( float4( ndc, 1.0 ), invP );
    float3 viewPos = viewH.xyz / viewH.w;

    float3 forward = normalize( camPos );
    float3 worldUp = abs( forward.y ) > 0.999 ? float3( 0, 0, 1 ) : float3( 0, 1, 0 );
    float3 right = normalize( cross( worldUp, forward ) );
    if ( all( right == 0 ) ) {
        worldUp = float3( 0, 0, 1 );
        right = normalize( cross( worldUp, forward ) );
    }
    float3 up = cross( forward, right );

    return camPos + -viewPos.x * right + viewPos.y * up + viewPos.z * forward;
}

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    VertexOutput output;
    // output.sv_position = mul( camera_buffer_data.mvp, float4( input.position, 1.0 ) );
    output.sv_position = float4( input.position, 1.0 );

    float4 cam_consts = camera_buffer_data.camera_constants;
    output.position = UnprojectNDC_SphericalCamera_ToWorld(
        float3( input.position.x, input.position.y, input.position.z ),
        camera_buffer_data.camera_pos.xyz, cam_consts.x, cam_consts.y, cam_consts.z, cam_consts.w );
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
    float3 p = march_pos * 0.00005f;

    float3 uvw = fract( p );
    //  float cumulus_map = noise_sum3D( float3( p.xz * 2.f + 100.f, 1.f ) );
    //  cumulus_map += 0.5;
    //  cumulus_map = smoothstep( 0.0, 1.0, cumulus_map );

    float cumulus_map = cumulus_map_LUT.Sample( linear_sampler, uvw.xz );

    float normY = -p.y * 0.5 + 0.5;
    float height_modulation = ( 0.5 * ( -normY + 1 ) ) / ( cumulus_map - 0.2 );
    float value2 = ( 0.5 * ( 10 * normY - 8 ) ) / ( cumulus_map - 0.2 );
    height_modulation
        = 0.5 + smoothstep( 0.6, 0.4, height_modulation ) * smoothstep( 1.0, 0.0, value2 );

    height_modulation
        = ( smoothstep( 3000, 3100, march_pos.y ) + smoothstep( 3500, 3100, march_pos.y ) ) - 1;

    // float worley = noise_sum3D_Worley( p.xyz * 1.5 + 100.f );
    // worley = smoothstep( 0.0, 0.8, worley ) - 0.2;

    // float low_freq_perlin = noise_sum3D( p.xyz * 1.5 + 100.f ) + 0.5;
    // low_freq_perlin = 0.8 * smoothstep( 0.3, 0.6, low_freq_perlin ) + 0.2;
    // float low_freq_noise = low_freq_perlin * ( 1.2 - worley ) * 0.5;
    // low_freq_noise *= 0.5;
    // low_freq_noise -= 0.25;
    uvw = fract( p * 4 );
    float low_freq_noise = low_freq_noise_LUT.Sample( linear_sampler, uvw );

    float low_freq_modulation = 1.f - height_modulation;

    // float high_freq_noise = noise_sum3D_Worley( p.xyz * 7 + 100.f );
    // high_freq_noise = 0.8 - 0.7 * smoothstep( 0.0, 0.8, high_freq_noise );
    uvw = fract( p * 12 );
    float high_freq_noise = low_freq_noise_LUT.Sample( linear_sampler, uvw );

    cumulus_map = lerp(
        cumulus_map, cumulus_map + 4 * low_freq_noise + 1 * high_freq_noise, low_freq_modulation );
    cumulus_map = smoothstep( 0.3, 0.6, cumulus_map ) * 2 - 1;

    return 0.5f * ( max( cumulus_map * height_modulation, 0 ) );
}

static const float3 light_dir = normalize( float3( 1, 1, 1 ) );

static const float absorption_coeff = 0.002f;

float estimate_transmittance( float3 march_pos )
{
    float3 local_dir = light_dir;

    if ( local_dir.y < 0.1 && local_dir.y > -0.1 ) {
        return 0.f;
    }

    float lower_plane_altitude = 3000;
    float higher_plane_altitude = 3500;
    if ( march_pos.y > higher_plane_altitude || march_pos.y < lower_plane_altitude ) {
        return 1.f;
    }

    float t_max;
    if ( march_pos.y > 0 ) {
        t_max = ( higher_plane_altitude - march_pos.y ) / local_dir.y;
    } else {
        t_max = ( lower_plane_altitude - march_pos.y ) / local_dir.y;
    }

    float t0 = 1e-3;
    float step_size = t_max / (float)16;

    float T = 1.0;
    for ( int i = 0; i < 16; ++i ) {
        float t = t0 + ( i + 0.5 ) * step_size; // midpoint sample
        float3 pos = march_pos + light_dir * t;
        // if outside the AABB you can break early, but caller will pass proper t_max
        float d = sample_density( pos );
        // Beer–Lambert per-step
        T *= exp( -SIGMA_T * d * step_size );

        if ( T <= 1e-4 ) {
            return 0.0;
        } // early out: essentially fully occluded
    }
    return saturate( T );
}

float PhaseHG( float cosTheta, float g )
{
    float g2 = g * g;
    float denom = pow( 1 + g2 - 2 * g * cosTheta, 1.5 );
    return ( 1 - g2 ) / ( 4 * 3.14159265 * denom );
}

static const float3 SUN_COLOR = float3( 1.0, 0.95, 0.85 );
static const float SUN_INTENSITY = 30.0; // brightness multiplier for direct light
static const float AMBIENT_INTENSITY = 0.2; // ambient skylight
static const float ALBEDO = 0.9; // single-scattering albedo
static const float SIGMA_T = 4.0; // extinction coefficient (tune for opacity)
static const float HG_G = 0.75; // Henyey-Greenstein asymmetry (-1..1), + forward scattering

float hash11( float n ) { return frac( sin( n ) * 43758.5453 ); }

float4 raymarch( float3 view, float3 origin )
{
    // float3 local_dir = normalize( mul( (float3x3)camera_buffer_data.inv_model, view ) );
    // float3 local_origin = mul( camera_buffer_data.inv_model, float4( origin, 1.0f ) ).xyz;
    float3 local_dir = float3( view.x, -view.y, view.z );
    float3 local_origin = origin;

    if ( local_dir.y < 0.1 ) {
        return float4( 0.f );
    }

    float lower_plane_altitude = 3000;
    float higher_plane_altitude = 3500;

    float t_min = ( lower_plane_altitude - origin.y ) / local_dir.y;
    float t_max = ( higher_plane_altitude - origin.y ) / local_dir.y;

    int steps = 40;
    float t = 0.0f;
    float step_size = ( t_max - t_min ) / (float)steps;
    float3 accumulated = float3( 0.0f );

    float throughput = 1.f;
    // jitter
    float jitter
        = ( rand_float( dot( local_origin.xyz, float3( 12.9898, 78.233, 45.164 ) ) ) - 0.5 )
        * step_size;
    for ( int i = 0; i < steps; ++i ) {
        float t = t_min + ( i + 0.5 ) * step_size + jitter;
        if ( throughput < 0.01 ) {
            break;
        }
        if ( t < t_min ) {
            continue;
        }
        if ( t > t_max ) {
            break;
        }
        float3 marched_pos = local_origin + local_dir * t;
        float density = sample_density( marched_pos );
        if ( density > 0.01 ) {

            float transmittance = estimate_transmittance( marched_pos );

            float3 local_light_dir
                = normalize( mul( (float3x3)camera_buffer_data.inv_model, light_dir ) );

            float cosTheta = dot( local_dir, local_light_dir );
            float phase
                = 0.95 * PhaseHG( cosTheta, 0.75 ) + 0.05 * PhaseHG( cosTheta, -0.2 ); // TTHG

            float3 direct_lighting
                = SUN_COLOR * SUN_INTENSITY * ALBEDO * phase * density * transmittance * step_size;
            accumulated += throughput * direct_lighting;
            throughput *= exp( -density * step_size * absorption_coeff );
        }
    }
    // Add background sky contribution attenuated by transmittance_along_ray
    float3 sky = float3( 0.6, 0.75, 0.95 ) * AMBIENT_INTENSITY;
    float3 color = accumulated + throughput * sky;

    if ( length( accumulated ) < 0.001 ) {
        return float4( 0.f );
    }

    // simple tonemap (ACES-lite / a gentle clamp)
    color = color / ( color + float3( 1.0, 1.0, 1.0 ) );
    color = pow( saturate( color ), float3( 1.0 / 2.2 ) ); // gamma

    return float4( color, 1.0 );
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
    float3 camera_pos = camera_buffer_data.camera_pos.rgb;

    float3 normal = in.normal;
    float3 light = normalize( float3( 1, 1, 1 ) );
    float3 view = normalize( world_pos - camera_pos );

    float4 output_color = raymarch( view, world_pos );

    if ( output_color.a < 0.1 ) {
        discard;
    }

    return output_color;
}


