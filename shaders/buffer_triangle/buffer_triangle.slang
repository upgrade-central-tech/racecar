struct VertexInput {
    float4 color    : COLOR0;
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct VertexOutput {
  float4 sv_position : SV_Position;
  float3 normal;
  float3 color;
};

struct CameraBufferData {
    float4x4 mvp;
    float3 color;
};

layout(binding = 0, set = 0) ConstantBuffer<CameraBufferData> camera_buffer_data; 

[shader("vertex")]
VertexOutput vs_main( VertexInput input ) {
  VertexOutput output;
  output.sv_position = mul(camera_buffer_data.mvp, float4(input.position, 1.0));
  output.color = input.color.rgb;
  output.normal = input.normal.rgb;
  return output;
}

[shader("fragment")]
float4 fs_main( VertexOutput vertex_in ) : SV_Target {
  float3 light = float3(1, 1, 1);
  float lambert = max(dot(normalize(light), normalize(vertex_in.normal)), 0.0);

  float3 out_color = lambert * vertex_in.color * (float3(camera_buffer_data.color) + 0.5f);

  return float4(out_color, 1.0);
}