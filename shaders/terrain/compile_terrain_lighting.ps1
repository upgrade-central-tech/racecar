# Assumes compiler is on your PATH (which it should be)

slangc.exe "$PSScriptRoot\terrain_lighting.slang" -target spirv -profile spirv_1_5 -emit-spirv-directly -fvk-use-entrypoint-name -entry cs_terrain_draw  -capability SPV_EXT_shader_atomic_float_add -o "$PSScriptRoot\cs_terrain_draw.spv"
slangc.exe "$PSScriptRoot\terrain_prepass.slang" -target spirv -profile spirv_1_4 -emit-spirv-directly -g2 -fvk-use-entrypoint-name -entry vs_main -entry fs_main -o "$PSScriptRoot\terrain_prepass.spv"