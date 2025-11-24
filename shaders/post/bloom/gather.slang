struct DebugData {
    float4 color;

    // Pack: roughness, metallic, clearcoat roughness, and clearcoat weight respsectively
    float4 packed_data0;
    float4 sun_direction;

    bool enable_albedo_map;
    bool enable_normal_map;
    bool enable_roughness_metal_map;
    bool normals_only;
    bool albedo_only;
    bool roughness_metal_only;

    bool enable_bloom;
}

layout( binding = 0, set = 0 ) Texture2D<float4> blurred_color; ///< Final blurred texture
layout( binding = 1, set = 0 ) Texture2D<float4> scene_color; ///< Original scene color
layout( binding = 2, set = 0 ) RWTexture2D<float4> output; ///< Final gathered place

layout( binding = 0, set = 1 ) ConstantBuffer<DebugData> debug;

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
func cs_main( uint2 thread_id: SV_DispatchThreadID )->void
{
    let pixel = thread_id.xy;
    let scene = scene_color.Load( uint3( pixel, 0 ) ).rgb;

    var final : float3;
    if ( debug.enable_bloom ) {
        // Simple additive blending
        let blurred = blurred_color.Load( uint3( pixel, 0 ) ).rgb;
        final = scene + blurred;
    } else {
        final = scene;
    }

    output[pixel] = float4( final, 1.f );
}


