import atmosphere;
import bindings;

struct VertexInput {
    float2 position : POSITION;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    float3 view_ray;
};

static const float3 GAMMA_FACTOR = float3( 1.f / 2.2f );
static const float3 GROUND_ALBEDO = float3( 0.f, 0.f, 0.04f );

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    let clip_position = float4( input.position, 0.f, 1.f );
    let view_position = mul( clip_position, uniforms.inverse_proj );
    let world_position = mul( view_position, uniforms.inverse_view );

    var output : VertexOutput;
    output.sv_position = clip_position;
    output.view_ray = world_position.xyz;

    return output;
}

[shader( "fragment" )]
float4 fs_main( VertexOutput input )
    : SV_Target
{
    return float4( get_sky_color( normalize( input.view_ray ) ), 1.f );
}


