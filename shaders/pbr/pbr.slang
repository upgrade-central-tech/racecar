struct VertexInput {
    float4 color    : COLOR0;
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct VertexOutput {
  float4 sv_position : SV_Position;
  float3 position;
  float3 normal;
  float3 color;
  float2 uv;
};

struct CameraBufferData {
    float4x4 mvp;
    float4x4 model;
    float4x4 inv_model;
    float3 color;
};

layout(binding = 0, set = 0) ConstantBuffer<CameraBufferData> camera_buffer_data;

[[vk::binding(1, 0)]]
Sampler2D test_texture;

static const float PI = 3.14159265359;

[shader("vertex")]
VertexOutput vs_main( VertexInput input ) {
  VertexOutput output;
  output.sv_position = mul(camera_buffer_data.mvp, float4(input.position, 1.0));
  output.position = mul(camera_buffer_data.model, float4(input.position, 1.0)).xyz;
  output.color = input.position.xyz;

  float3x3 normal_matrix = (float3x3)transpose(camera_buffer_data.inv_model);
  output.normal = mul(normal_matrix, input.normal);
  output.uv = input.uv;

  return output;
}

float DistributionGGX(float n_dot_h, float roughness) {
  float alpha = roughness * roughness;
  float alpha2 = max(alpha * alpha, 0.001f);

  float n_dot_h_2 = n_dot_h * n_dot_h;
  float denom = (n_dot_h_2) * (alpha2 - 1.0f)  + 1.0f;

  return alpha2 / (denom * denom); 
}

float GeometrySchlick(float theta, float k) {
  return theta / ( theta * (1.0f - k) + k );
}

float GeometrySmith(float n_dot_v, float n_dot_l, float roughness) {
  float alpha = roughness * roughness;
  float num = (alpha + 1.0f);
  float k = (num * num) / 8.0f;

  return GeometrySchlick(n_dot_v, k) * GeometrySchlick(n_dot_l, k);
}

float3 FresnelSchlick(float cos_theta, float3 F0) {
    return F0 + (1.0f - F0) * pow(1.0f - cos_theta, 5.0f);
}

[shader("fragment")]
float4 fs_main( VertexOutput in ) : SV_Target {
  // Basic vectors
  float3 camera_pos = float3(0, 0, 3);
  float3 normal = normalize(in.normal);
  float3 view = normalize(camera_pos - in.position);
  float3 half = normalize(normal + view);
  
  float3 light_pos = float3(1, 2, 1);
  float3 light = normalize(light_pos);

  float3 base_albedo = 0.5f * (test_texture.Sample(in.uv).rgb) + 0.5f * normal;

  // Material properties
  float roughness = 0.001f;
  float metallic = 0.01f;
  float3 F0 = float3(0.04f);
  F0 = lerp(F0, base_albedo, metallic);

  // Essential dot products
  float n_dot_v = max(dot(normal, view), 0.0f);
  float n_dot_h = max(dot(normal, half), 0.0f);
  float n_dot_l = max(dot(normal, light), 0.0f);

  // Diffuse reflection
  float3 diffuse_albedo = base_albedo;
  float3 diffuse = diffuse_albedo;

  // Specular reflection
  float NDF = DistributionGGX(n_dot_h, roughness);
  float G = GeometrySmith(n_dot_v, n_dot_l, roughness);
  float3 F = FresnelSchlick(n_dot_h, F0);

  float3 specular = (NDF * F * G) / (4.0f * n_dot_v * n_dot_l);

  float3 out_color = (1.0f - F) * diffuse + specular;
  out_color *= n_dot_l;

  // Gamma
  out_color = out_color / (out_color + float3(1.0f));
  out_color = pow(out_color, float3(1.0f / 2.2f));

  return float4(out_color, 1.0);
}