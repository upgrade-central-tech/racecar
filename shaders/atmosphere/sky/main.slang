import atmosphere;
import bindings;

struct VertexInput {
    float2 position : POSITION;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    float3 view_ray;
};

[shader( "vertex" )]
VertexOutput vs_main( VertexInput input )
{
    let clip_position = float4( input.position, 0.99f, 1.f );
    let view_position = mul( uniforms.inverse_proj, clip_position );
    let world_position = mul( uniforms.inverse_view, float4( view_position.xyz, 0.f ) );

    var output : VertexOutput;
    output.sv_position = clip_position;
    output.view_ray = world_position.xyz;

    return output;
}

[shader( "fragment" )]
float4 fs_main( VertexOutput input )
    : SV_Target
{
    float3 color = get_sky_color( normalize( input.view_ray ) );
    return float4( color, 1.f );
}


