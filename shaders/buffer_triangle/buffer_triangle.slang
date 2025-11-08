struct VertexInput {
    float4 color    : COLOR0;
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct VertexOutput {
  float4 sv_position : SV_Position;
  float3 color;
};

struct CameraBufferData {
    mat4x4 etc..
    float3 color;
};

layout(binding = 0, set = 0) ConstantBuffer<CameraBufferData> camera_buffer_data; 

[shader("vertex")]
VertexOutput vs_main( VertexInput input ) {
  VertexOutput output;
  output.sv_position = float4(input.position, 1.0);
  output.color = input.color.rgb;

  return output;
}

[shader("fragment")]
float4 fs_main( VertexOutput vertex_in ) : SV_Target {
  return float4(float3(camera_buffer_data.color), 1.0);
}