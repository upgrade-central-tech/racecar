# Assumes compiler is on your PATH (which it should be)
../../../slang/bin/slangc.exe "$PSScriptRoot\clouds.slang" -target spirv -profile spirv_1_4 -g2 -emit-spirv-directly -fvk-use-entrypoint-name -entry vs_main -entry fs_main -o "$PSScriptRoot\clouds.spv"

../../../slang/bin/slangc.exe "$PSScriptRoot\cloud_noise.slang" -target spirv -profile spirv_1_5 -emit-spirv-directly -fvk-use-entrypoint-name -entry cs_generate_low_frequency  -capability SPV_EXT_shader_atomic_float_add -o "$PSScriptRoot\cs_generate_low_frequency.spv"
../../../slang/bin/slangc.exe "$PSScriptRoot\cloud_noise.slang" -target spirv -profile spirv_1_5 -emit-spirv-directly -fvk-use-entrypoint-name -entry cs_generate_high_frequency  -capability SPV_EXT_shader_atomic_float_add -o "$PSScriptRoot\cs_generate_high_frequency.spv"
../../../slang/bin/slangc.exe "$PSScriptRoot\cloud_noise.slang" -target spirv -profile spirv_1_5 -emit-spirv-directly -fvk-use-entrypoint-name -entry cs_generate_cumulus  -capability SPV_EXT_shader_atomic_float_add -o "$PSScriptRoot\cs_generate_cumulus.spv"

../../../slang/bin/slangc.exe "$PSScriptRoot\cloud_composite.slang" -target spirv -profile spirv_1_5 -emit-spirv-directly -fvk-use-entrypoint-name -entry vs_main -entry fs_main -o "$PSScriptRoot\cloud_composite.spv"