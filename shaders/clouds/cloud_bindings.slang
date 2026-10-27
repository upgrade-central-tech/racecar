module cloud_bindings;

#include "../common.slang"
#include "cloud_uniforms.slang"

layout( binding = 0, set = 0 ) public ConstantBuffer<CloudUniformData> clouds_buffer_data;
layout( binding = 1, set = 0 ) public ConstantBuffer<DebugData> debug_data;

layout( binding = 0, set = 1 ) public Texture3D<float> low_freq_noise_LUT;
layout( binding = 1, set = 1 ) public Texture3D<float> high_freq_noise_LUT;
layout( binding = 2, set = 1 ) public Texture2D<float> cumulus_map_LUT;

layout( binding = 0, set = 2 ) public SamplerState linear_sampler;
layout( binding = 1, set = 2 ) public SamplerState linear_mirrored_repeat_sampler;


