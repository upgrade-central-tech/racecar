
struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT0;
    float2 uv : TEXCOORD0;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    float4 uv;
};

// Material stuff strictly
layout( binding = 0, set = 1 ) Texture2D<float4> albedo_map;
layout( binding = 1, set = 1 ) Texture2D<float4> normal_map;
layout( binding = 2, set = 1 ) Texture2D<float4> metallic_roughness_map;

// Cubemaps + LUTs
layout( binding = 0, set = 2 ) TextureCube<float4> cubemap;
layout( binding = 1, set = 2 ) Texture2D<float2> BRDF_LUT;

layout( binding = 0, set = 3 ) SamplerState nearest_sampler;

static const float PI = 3.14159265359;

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    VertexOutput output;
    output.sv_position = float4( input.position, 0.1f, 1.f );
    output.uv = ( input.position + 1.f ) * 0.5f;
    return output;
}

[shader( "fragment" )]
float4 fs_main( VertexOutput in ) : SV_Target
{
    return in.sv_position;
}

