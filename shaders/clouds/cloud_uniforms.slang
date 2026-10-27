#pragma once

public struct CloudUniformData {
    public float4x4 inverse_proj;
    public float4x4 inverse_view;
    public float3 camera_position;
    public float cloud_offset_x;
    public float3 sun_direction;
    public float cloud_offset_y;
    // extinction, erosion, null, null
    public float4 cloud_params;
};
