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
    float4x4 view_mat;
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
layout( binding = 1, set = 2 ) SamplerState linear_mirrored_repeat_sampler;

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

    float cumulus_map = cumulus_map_LUT.Sample( linear_mirrored_repeat_sampler, p.xz );
    float3 uvw = fract( p );

    float normY = -p.y * 0.5 + 0.5;
    float height_modulation = ( 0.5 * ( -normY + 1 ) ) / ( cumulus_map - 0.2 );
    float value2 = ( 0.5 * ( 10 * normY - 8 ) ) / ( cumulus_map - 0.2 );
    height_modulation
        = 0.5 + smoothstep( 0.6, 0.4, height_modulation ) * smoothstep( 1.0, 0.0, value2 );
    height_modulation
        = ( smoothstep( 3000, 3100, march_pos.y ) + smoothstep( 3500, 3100, march_pos.y ) ) - 1;

    float low_freq_noise = low_freq_noise_LUT.Sample( linear_mirrored_repeat_sampler, p * 4 );

    float low_freq_modulation = 1.f - height_modulation;

    float high_freq_noise = low_freq_noise_LUT.Sample( linear_mirrored_repeat_sampler, p * 12 );

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

float offset_sphere_intersection( float3 view, float3 origin, float surface_level, float rad )
{
    // a Triangle forms between the earth's core, the current position, and the intersection point
    float PI = 3.141592654;
    float obtuse_angle = PI;
    if ( length( float3( view.x, 0, view.z ) ) > 0.01 ) {
        float3 ground_vector = normalize( float3( view.x, 0, view.z ) );
        float angle_to_ground = acos( dot( view, ground_vector ) );
        obtuse_angle = angle_to_ground + PI / 2;
    }

    // use law of sines to find t
    float atmo_angle = asin( surface_level * sin( obtuse_angle ) / rad );
    float core_angle = PI - atmo_angle - obtuse_angle;
    float t = rad * sin( core_angle ) / sin( obtuse_angle );

    return t;
}

float4 raymarch( float3 view, float3 origin )
{
    float PI = 3.141592654f;
    view.y *= -1;

    float theta = acos( saturate( view.y ) );
    // theta = clamp( theta, 0.0, radians( 80 ) );

    float lerp = ( theta ) / PI * 2.f;

    float thetaRemap = lerp * radians( 80 ); 
    float phi = atan2( view.z, view.x );
    

    float sinTheta = sin( thetaRemap );
    float3 local_dir = float3( sinTheta * cos( phi ), cos( thetaRemap ), sinTheta * sin( phi ) );
    float3 local_origin = origin;

    if ( view.y < 0.0 ) {
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

    // make clouds opaque at the sun.
    if ( dot( local_dir, -light_dir ) > 0.9985 ) {
        return float4( color, 1.0 );
    }

    if ( view.y < 0.1 ) {
        throughput *= view.y * 10.f;
    }

    return float4( color, max( 0, throughput ) );
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

    return float4( output_color.rgb * output_color.a, output_color.a );
}


