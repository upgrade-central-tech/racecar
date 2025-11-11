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
    float4x4 model;
    float4x4 inv_model;
    float3 color;
};

layout(binding = 0, set = 0) ConstantBuffer<CameraBufferData> camera_buffer_data; 

[shader("vertex")]
VertexOutput vs_main( VertexInput input ) {
  VertexOutput output;
  output.sv_position = mul(camera_buffer_data.mvp, float4(input.position, 1.0));
  output.color = input.position.xyz;

  float3x3 normal_matrix = (float3x3)transpose(camera_buffer_data.inv_model);
  output.normal = mul(normal_matrix, input.normal);

  return output;
}

[shader("fragment")]
float4 fs_main( VertexOutput vertex_in ) : SV_Target {
  float3 normal = normalize(vertex_in.normal);
  return float4(normal, 1.0);
}