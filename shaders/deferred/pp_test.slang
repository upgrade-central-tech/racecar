struct CameraBufferData {
    float4x4 mvp;
    float4x4 model;
    float4x4 inv_model;
    float4 camera_pos;
    // Pack: near plane, far plane, aspect ratio, fov_Y
    float4 camera_constants;
};

layout( binding = 0, set = 0 ) Texture2D<float4> scene_color;
layout( binding = 1, set = 0 ) Texture2D<float> GBuffer_Depth;
layout( binding = 2, set = 0 ) Texture2DMS<float> GBuffer_DepthMS;
layout( binding = 3, set = 0 ) Texture2D<float4> GBuffer_Normal;
layout( binding = 4, set = 0 ) RWTexture2D<float4> out_color;
layout( binding = 0, set = 1 ) ConstantBuffer<CameraBufferData> camera_data;

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void cs_pp_test( uint2 thread_id: SV_DispatchThreadID )
{
    uint2 pixel = thread_id.xy;

    float3 in_color = scene_color.Load( uint3( pixel, 0 ) ).rgb;
    float in_depth = GBuffer_Depth.Load( uint3( pixel, 0 ) ).r;
    float in_depthMS = GBuffer_DepthMS.Load( uint3( pixel, 0 ) ).r;

    out_color[pixel] = float4( 1.f - in_color, 1.0f );
}


