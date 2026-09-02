#include "../../utils.slang"

static const int MAX_DEBUG_TEXTURES = 8;

struct DebugTextureSettings {
    // 0 is off, >= 1 is index
    int index;
    float exposure;

    float blur;
};

layout( binding = 0, set = 0 ) Texture2D<float4> debug_textures[MAX_DEBUG_TEXTURES];

layout( binding = 0, set = 1 ) RWTexture2D<float4> out_color;
layout( binding = 1, set = 1 ) SamplerState linear_sampler;
layout( binding = 2, set = 1 ) ConstantBuffer<DebugTextureSettings> settings;

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
func cs_debug_texture( uint2 thread_id: SV_DispatchThreadID )->void
{
    if ( settings.index <= 0 ) {
        return;
    }

    uint width;
    uint height;
    out_color.GetDimensions( width, height );

    let pixel = thread_id.xy;
    if ( pixel.x >= width || pixel.y >= height ) {
        return;
    }

    let uv = ( float2( pixel ) + 0.5f ) / float2( width, height );
    let texel = debug_textures[settings.index - 1].SampleLevel( linear_sampler, uv, settings.blur );

    out_color[pixel] = float4( linear_to_srgb( texel.rgb * settings.exposure ), 1.0f );
}
