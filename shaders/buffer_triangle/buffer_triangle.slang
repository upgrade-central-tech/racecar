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

[shader("vertex")]
VertexOutput vs_main( VertexInput input ) {
  VertexOutput output;
  output.sv_position = float4(input.position, 1.0);
  output.color = input.color.rgb;

  return output;
}

[shader("fragment")]
float4 fs_main( VertexOutput vertex_in ) : SV_Target {
  return float4(vertex_in.color, 1.0);
}